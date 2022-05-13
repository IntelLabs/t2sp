#include "InjectHostDevBufferCopies.h"

#include "CodeGen_GPU_Dev.h"
#include "Debug.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "Substitute.h"
#include "../../t2s/src/BuildCallRelation.h"
#include "../../t2s/src/DebugPrint.h"

#include <map>

namespace Halide {
namespace Internal {

using std::set;
using std::string;
using std::vector;

Stmt call_extern_and_assert(const string &name, const vector<Expr> &args) {
    Expr call = Call::make(Int(32), name, args, Call::Extern);
    string call_result_name = unique_name(name + "_result");
    Expr call_result_var = Variable::make(Int(32), call_result_name);
    return LetStmt::make(call_result_name, call,
                         AssertStmt::make(EQ::make(call_result_var, 0), call_result_var));
}

namespace {

class FindBufferUsage : public IRVisitor {
    using IRVisitor::visit;

    void visit(const Load *op) override {
        IRVisitor::visit(op);
        if (op->name == buffer) {
            devices_touched.insert(current_device_api);
        }
    }

    void visit(const Store *op) override {
        IRVisitor::visit(op);
        if (op->name == buffer) {
            devices_touched.insert(current_device_api);
            devices_writing.insert(current_device_api);
        }
    }

    bool is_buffer_var(Expr e) {
        const Variable *var = e.as<Variable>();
        return var && (var->name == buffer + ".buffer");
    }

    void visit(const Call *op) override {
        if (op->is_intrinsic(Call::image_load)) {
            internal_assert(!op->args.empty());
            if (is_buffer_var(op->args[1])) {
                devices_touched.insert(current_device_api);
            }
            for (size_t i = 0; i < op->args.size(); i++) {
                if (i == 1) continue;
                op->args[i].accept(this);
            }
        } else if (op->is_intrinsic(Call::image_store)) {
            internal_assert(!op->args.empty());
            if (is_buffer_var(op->args[1])) {
                devices_touched.insert(current_device_api);
                devices_writing.insert(current_device_api);
            }
            for (size_t i = 0; i < op->args.size(); i++) {
                if (i == 1) continue;
                op->args[i].accept(this);
            }
        } else if (op->is_intrinsic(Call::debug_to_file)) {
            internal_assert(op->args.size() == 3);
            if (is_buffer_var(op->args[2])) {
                devices_touched.insert(current_device_api);
                devices_writing.insert(current_device_api);
            }
        } else if (op->is_extern() && op->func.defined()) {
            // This is a call to an extern stage
            Function f(op->func);

            internal_assert((f.extern_arguments().size() + f.outputs()) == op->args.size())
                << "Mismatch between args size and extern_arguments size in call to "
                << op->name << "\n";

            // Check each buffer arg
            for (size_t i = 0; i < op->args.size(); i++) {
                if (is_buffer_var(op->args[i])) {
                    DeviceAPI extern_device_api = f.extern_function_device_api();
                    devices_touched_by_extern.insert(extern_device_api);
                    if (i >= f.extern_arguments().size()) {
                        // An output. The extern stage is responsible
                        // for dealing with any device transitions for
                        // inputs.
                        devices_touched.insert(extern_device_api);
                        devices_writing.insert(extern_device_api);
                    }
                } else {
                    op->args[i].accept(this);
                }
            }
        } else {
            IRVisitor::visit(op);
        }
    }

    void visit(const For *op) override {
        internal_assert(op->device_api != DeviceAPI::Default_GPU)
            << "A GPU API should have been selected by this stage in lowering\n";
        DeviceAPI old = current_device_api;
        if (op->device_api != DeviceAPI::None) {
            current_device_api = op->device_api;
        }
        IRVisitor::visit(op);
        current_device_api = old;
    }

    string buffer;
    DeviceAPI current_device_api;

public:
    std::set<DeviceAPI> devices_writing, devices_touched;
    // Any buffer passed to an extern stage may have had its dirty
    // bits and device allocation messed with.
    std::set<DeviceAPI> devices_touched_by_extern;

    FindBufferUsage(const std::string &buf, DeviceAPI d)
        : buffer(buf), current_device_api(d) {
    }
};

// Inject the device copies, mallocs, and dirty flag setting for a
// single allocation. Sticks to the same loop level as the original
// allocation and treats the stmt as a serial sequence of leaf
// stmts. We walk this sequence of leaves, tracking what we know about
// the buffer as we go, sniffing usage within each leaf using
// FindBufferUsage, and injecting device buffer logic as needed.
class InjectBufferCopiesForSingleBuffer : public IRMutator {
    using IRMutator::visit;

    // The buffer being managed
    string buffer;

    bool is_external;

    enum FlagState {
        Unknown,
        False,
        True
    };

    struct State {
        // What do we know about the dirty flags and the existence of a device allocation?
        FlagState device_dirty = Unknown, host_dirty = Unknown, device_allocation_exists = Unknown;

        // If it exists on a known device API, which device does it exist
        // on? Meaningless if device_allocation_exists is not True.
        DeviceAPI current_device = DeviceAPI::None;

        void union_with(const State &other) {
            if (device_dirty != other.device_dirty) {
                device_dirty = Unknown;
            }
            if (host_dirty != other.host_dirty) {
                host_dirty = Unknown;
            }
            if (device_allocation_exists != other.device_allocation_exists ||
                other.current_device != current_device) {
                device_allocation_exists = Unknown;
                current_device = DeviceAPI::None;
            }
        }
    } state;

    Expr buffer_var() {
        return Variable::make(type_of<struct halide_buffer_t *>(), buffer + ".buffer");
    }

    Stmt make_device_malloc(DeviceAPI target_device_api) {
        Expr device_interface = make_device_interface_call(target_device_api);
        Stmt device_malloc = call_extern_and_assert("halide_device_malloc",
                                                    {buffer_var(), device_interface});
        return device_malloc;
    }

    Stmt make_copy_to_host() {
        return call_extern_and_assert("halide_copy_to_host", {buffer_var()});
    }

    Stmt make_copy_to_device(DeviceAPI target_device_api) {
        Expr device_interface = make_device_interface_call(target_device_api);
        return call_extern_and_assert("halide_copy_to_device", {buffer_var(), device_interface});
    }

    Stmt make_host_dirty() {
        return Evaluate::make(Call::make(Int(32), Call::buffer_set_host_dirty,
                                         {buffer_var(), const_true()}, Call::Extern));
    }

    Stmt make_device_dirty() {
        return Evaluate::make(Call::make(Int(32), Call::buffer_set_device_dirty,
                                         {buffer_var(), const_true()}, Call::Extern));
    }

    Stmt make_device_free() {
        return call_extern_and_assert("halide_device_free", {buffer_var()});
    }

    Stmt do_copies(Stmt s) {
        // Sniff what happens to the buffer inside the stmt
        FindBufferUsage finder(buffer, DeviceAPI::Host);
        s.accept(&finder);

        // Insert any appropriate copies/allocations before, and set
        // dirty flags after. Do not recurse into the stmt.

        // First figure out what happened
        bool touched_on_host = finder.devices_touched.count(DeviceAPI::Host);
        bool touched_on_device = finder.devices_touched.size() > (touched_on_host ? 1 : 0);
        bool written_on_host = finder.devices_writing.count(DeviceAPI::Host);
        bool written_on_device = finder.devices_writing.size() > (touched_on_host ? 1 : 0);

        DeviceAPI touching_device = DeviceAPI::None;
        for (DeviceAPI d : finder.devices_touched) {
            // TODO: looks dubious, but removing causes crashes in correctness_debug_to_file
            // with target=host-metal.
            if (d == DeviceAPI::Host) continue;
            internal_assert(touching_device == DeviceAPI::None)
                << "Buffer " << buffer << " was touched on multiple devices within a single leaf Stmt!\n";
            touching_device = d;
        }

        // Then figure out what to do
        bool needs_device_malloc = (touched_on_device &&
                                    (state.device_allocation_exists != True));

        bool needs_device_flip = (state.device_allocation_exists != False &&
                                  state.current_device != touching_device &&
                                  state.current_device != DeviceAPI::None &&
                                  touching_device != DeviceAPI::None &&
                                  !is_external);

        // TODO: If only written on device, and entirely clobbered on
        // device, a copy-to-device is not actually necessary.
        bool needs_copy_to_device = (touched_on_device &&
                                     ((state.host_dirty != False) ||
                                      needs_device_flip));

        if (needs_copy_to_device) {
            // halide_copy_to_device already does a halide_device_malloc if necessary.
            needs_device_malloc = false;
        }

        // Device flips go via host memory
        bool needs_copy_to_host = ((touched_on_host || needs_device_flip) &&
                                   (state.device_dirty != False));

        bool needs_host_dirty = (written_on_host &&
                                 (state.host_dirty != True));

        bool needs_device_dirty = (written_on_device &&
                                   (state.device_dirty != True));

        vector<Stmt> stmts;

        // Then do it, updating what we know about the buffer
        if (needs_copy_to_host) {
            stmts.push_back(make_copy_to_host());
            state.device_dirty = False;
        }

        // When flipping a buffer between devices, we need to free the
        // old device memory before allocating the new one.
        if (needs_device_flip) {
            stmts.push_back(make_host_dirty());
            stmts.push_back(make_device_free());
            state.device_allocation_exists = False;
            state.device_dirty = False;
        }

        if (needs_copy_to_device) {
            stmts.push_back(make_copy_to_device(touching_device));
            state.host_dirty = False;
            state.device_allocation_exists = True;
            state.current_device = touching_device;
        }

        if (needs_device_malloc) {
            stmts.push_back(make_device_malloc(touching_device));
            state.device_allocation_exists = True;
            state.current_device = touching_device;
        }

        stmts.push_back(s);

        if (needs_host_dirty) {
            stmts.push_back(make_host_dirty());
            state.host_dirty = True;
        }

        if (needs_device_dirty) {
            stmts.push_back(make_device_dirty());
            state.device_dirty = True;
        }

        s = Block::make(stmts);

        if (!finder.devices_touched_by_extern.empty()) {
            // This buffer was passed to an extern stage. Unless we
            // explicitly marked it after the stmt ran, we no longer
            // know the state of the dirty bits.
            if (!needs_host_dirty) {
                state.host_dirty = Unknown;
            }
            if (!needs_device_dirty) {
                state.device_dirty = Unknown;
            }
            // Also, the extern stage may have gifted a host
            // allocation, or flipped the buffer to another device.
            state.device_allocation_exists = Unknown;
            state.current_device = DeviceAPI::None;
        }

        return s;
    }

    // We want to break things down into a serial sequence of leaf
    // stmts, and possibly do copies and update state around each
    // leaf.

    Stmt visit(const For *op) override {
        // All copies happen at the same loop level as the allocation.
        return do_copies(op);
    }

    Stmt visit(const Fork *op) override {
        return do_copies(op);
    }

    Stmt visit(const Evaluate *op) override {
        return do_copies(op);
    }

    Stmt visit(const LetStmt *op) override {
        // If op->value uses the buffer, we need to treat this as a
        // single leaf. Otherwise we can recurse.
        FindBufferUsage finder(buffer, DeviceAPI::Host);
        op->value.accept(&finder);
        if (finder.devices_touched.empty() &&
            finder.devices_touched_by_extern.empty()) {
            return IRMutator::visit(op);
        } else {
            return do_copies(op);
        }
    }

    Stmt visit(const AssertStmt *op) override {
        return do_copies(op);
    }

    // Check if a stmt has any for loops (and hence possible device
    // transitions).
    class HasLoops : public IRVisitor {
        using IRVisitor::visit;
        void visit(const For *op) override {
            result = true;
        }

    public:
        bool result = false;
    };

    Stmt visit(const Block *op) override {
        // If both sides of the block have no loops (and hence no
        // device transitions), treat it as a single leaf. This stops
        // host dirties from getting in between blocks of store stmts
        // that could be interleaved.
        HasLoops loops;
        op->accept(&loops);
        if (loops.result) {
            return IRMutator::visit(op);
        } else {
            return do_copies(op);
        }
    }

    Stmt visit(const Store *op) override {
        return do_copies(op);
    }

    Stmt visit(const IfThenElse *op) override {
        State old = state;
        Stmt then_case = mutate(op->then_case);
        State then_state = state;
        state = old;
        Stmt else_case = mutate(op->else_case);
        state.union_with(then_state);
        return IfThenElse::make(op->condition, then_case, else_case);
    }

public:
    InjectBufferCopiesForSingleBuffer(const std::string &b, bool e)
        : buffer(b), is_external(e) {
        if (is_external) {
            // The state of the buffer is totally unknown, which is
            // the default constructor for this->state
        } else {
            // This is a fresh allocation
            state.device_allocation_exists = False;
            state.device_dirty = False;
            state.host_dirty = False;
            state.current_device = DeviceAPI::None;
        }
    }
};

// Find the last use of a given buffer, which will used later for injecting
// device free calls.
class FindLastUse : public IRVisitor {
public:
    Stmt last_use;

    FindLastUse(const string &b)
        : buffer(b) {
    }

private:
    string buffer;

    using IRVisitor::visit;

    void check_and_record_last_use(Stmt s) {
        // Sniff what happens to the buffer inside the stmt
        FindBufferUsage finder(buffer, DeviceAPI::Host);
        s.accept(&finder);

        if (!finder.devices_touched.empty() ||
            !finder.devices_touched_by_extern.empty()) {
            last_use = s;
        }
    }

    // We break things down into a serial sequence of leaf
    // stmts similar to InjectBufferCopiesForSingleBuffer.
    void visit(const For *op) override {
        check_and_record_last_use(op);
    }

    void visit(const Fork *op) override {
        check_and_record_last_use(op);
    }

    void visit(const Evaluate *op) override {
        check_and_record_last_use(op);
    }

    void visit(const LetStmt *op) override {
        // If op->value uses the buffer, we need to treat this as a
        // single leaf. Otherwise we can recurse.
        FindBufferUsage finder(buffer, DeviceAPI::Host);
        op->value.accept(&finder);
        if (finder.devices_touched.empty() &&
            finder.devices_touched_by_extern.empty()) {
            IRVisitor::visit(op);
        } else {
            check_and_record_last_use(op);
        }
    }

    void visit(const AssertStmt *op) override {
        check_and_record_last_use(op);
    }

    void visit(const Store *op) override {
        check_and_record_last_use(op);
    }

    void visit(const IfThenElse *op) override {
        check_and_record_last_use(op);
    }
};

// Inject the buffer-handling logic for all internal
// allocations. Inputs and outputs are handled below.
class InjectBufferCopies : public IRMutator {
    using IRMutator::visit;

    // Inject the registration of a device destructor just after the
    // .buffer symbol is defined (which is safely before the first
    // device_malloc).
    class InjectDeviceDestructor : public IRMutator {
        using IRMutator::visit;

        Stmt visit(const LetStmt *op) override {
            if (op->name == buffer) {
                Expr buf = Variable::make(type_of<struct halide_buffer_t *>(), buffer);
                Stmt destructor =
                    Evaluate::make(Call::make(Handle(), Call::register_destructor,
                                              {Expr("halide_device_free_as_destructor"), buf}, Call::Intrinsic));
                Stmt body = Block::make(destructor, op->body);
                return LetStmt::make(op->name, op->value, body);
            } else {
                return IRMutator::visit(op);
            }
        }

        string buffer;

    public:
        InjectDeviceDestructor(string b)
            : buffer(b) {
        }
    };

    // Find the let stmt that defines the .buffer and insert inside of
    // it a combined host/dev allocation, a destructor registration,
    // and an Allocate node that takes its host field from the
    // .buffer.
    class InjectCombinedAllocation : public IRMutator {
        using IRMutator::visit;

        Stmt visit(const LetStmt *op) override {
            if (op->name == buffer + ".buffer") {
                Expr buf = Variable::make(type_of<struct halide_buffer_t *>(), buffer + ".buffer");
                Stmt body = op->body;

                // The allocate node is innermost
                Expr host = Call::make(Handle(), Call::buffer_get_host, {buf}, Call::Extern);
                body = Allocate::make(buffer, type, MemoryType::Heap, extents, condition, body,
                                      host, "halide_device_host_nop_free");

                // Then the destructor
                Stmt destructor =
                    Evaluate::make(Call::make(Handle(), Call::register_destructor,
                                              {Expr("halide_device_and_host_free_as_destructor"), buf},
                                              Call::Intrinsic));
                body = Block::make(destructor, body);

                // Then the device_and_host malloc
                Expr device_interface = make_device_interface_call(device_api);
                Stmt device_malloc = call_extern_and_assert("halide_device_and_host_malloc",
                                                            {buf, device_interface});
                if (!is_one(condition)) {
                    device_malloc = IfThenElse::make(condition, device_malloc);
                }
                body = Block::make(device_malloc, body);

                // In the value, we want to use null for the initial value of the host field.
                Expr value = substitute(buffer, reinterpret(Handle(), make_zero(UInt(64))), op->value);

                // Rewrap the letstmt
                return LetStmt::make(op->name, value, body);
            } else {
                return IRMutator::visit(op);
            }
        }

        string buffer;
        Type type;
        vector<Expr> extents;
        Expr condition;
        DeviceAPI device_api;

    public:
        InjectCombinedAllocation(string b, Type t, vector<Expr> e, Expr c, DeviceAPI d)
            : buffer(b), type(t), extents(e), condition(c), device_api(d) {
        }
    };

    class FreeAfterLastUse : public IRMutator {
        Stmt last_use;
        Stmt free_stmt;

    public:
        bool success = false;
        using IRMutator::mutate;

        Stmt mutate(const Stmt &s) override {
            if (s.same_as(last_use)) {
                internal_assert(!success);
                success = true;
                return Block::make(last_use, free_stmt);
            } else {
                return IRMutator::mutate(s);
            }
        }

        FreeAfterLastUse(Stmt s, Stmt f)
            : last_use(s), free_stmt(f) {
        }
    };

    Stmt visit(const Allocate *op) override {
        FindBufferUsage finder(op->name, DeviceAPI::Host);
        op->body.accept(&finder);

        bool touched_on_host = finder.devices_touched.count(DeviceAPI::Host);
        bool touched_on_device = finder.devices_touched.size() > (touched_on_host ? 1 : 0);

        if (!touched_on_device && finder.devices_touched_by_extern.empty()) {
            // Boring.
            return IRMutator::visit(op);
        }

        Stmt body = mutate(op->body);

        InjectBufferCopiesForSingleBuffer injector(op->name, false);
        body = injector.mutate(body);

        string buffer_name = op->name + ".buffer";
        Expr buffer = Variable::make(Handle(), buffer_name);

        // Device what type of allocation to make.

        if (touched_on_host && finder.devices_touched.size() == 2) {
            // Touched on a single device and the host. Use a combined allocation.
            DeviceAPI touching_device = DeviceAPI::None;
            for (DeviceAPI d : finder.devices_touched) {
                if (d != DeviceAPI::Host) {
                    touching_device = d;
                }
            }

            // Make a device_and_host_free stmt

            FindLastUse last_use(op->name);
            body.accept(&last_use);
            if (last_use.last_use.defined()) {
                Stmt device_free = call_extern_and_assert("halide_device_and_host_free", {buffer});
                FreeAfterLastUse free_injecter(last_use.last_use, device_free);
                body = free_injecter.mutate(body);
                internal_assert(free_injecter.success);
            }

            return InjectCombinedAllocation(op->name, op->type, op->extents,
                                            op->condition, touching_device)
                .mutate(body);
        } else {
            // Only touched on host but passed to an extern stage, or
            // only touched on device, or touched on multiple
            // devices. Do separate device and host allocations.

            // Add a device destructor
            body = InjectDeviceDestructor(buffer_name).mutate(body);

            // Make a device_free stmt

            FindLastUse last_use(op->name);
            body.accept(&last_use);
            if (last_use.last_use.defined()) {
                Stmt device_free = call_extern_and_assert("halide_device_free", {buffer});
                FreeAfterLastUse free_injecter(last_use.last_use, device_free);
                body = free_injecter.mutate(body);
                internal_assert(free_injecter.success);
            }

            Expr condition = op->condition;
            bool touched_on_one_device = !touched_on_host && finder.devices_touched.size() == 1 &&
                                         (finder.devices_touched_by_extern.empty() ||
                                          (finder.devices_touched_by_extern.size() == 1 &&
                                           *(finder.devices_touched.begin()) == *(finder.devices_touched_by_extern.begin())));
            if (touched_on_one_device) {
                condition = const_false();
                // There's no host allocation, so substitute any
                // references to it (e.g. the one in the make_buffer
                // call) with NULL.
                body = substitute(op->name, reinterpret(Handle(), make_zero(UInt(64))), body);
            }

            return Allocate::make(op->name, op->type, op->memory_type, op->extents,
                                  condition, body, op->new_expr, op->free_function);
        }
    }

    Stmt visit(const For *op) override {
        if (op->device_api != DeviceAPI::Host &&
            op->device_api != DeviceAPI::None) {
            // Don't enter device loops
            return op;
        } else {
            return IRMutator::visit(op);
        }
    }

public:
    InjectBufferCopies(const Target &t) : target(t) { }

private:
    const Target &target;
};

// Find the site in the IR where we want to inject the copies/dirty
// flags for the inputs and outputs. It's the innermost IR node that
// contains all ProducerConsumer nodes. Often this is the outermost
// ProducerConsumer node. Sometimes it's a Block containing a pair of
// them.
class FindOutermostProduce : public IRVisitor {
    using IRVisitor::visit;

    void visit(const Block *op) override {
        op->first.accept(this);
        if (result.defined()) {
            result = op;
        } else {
            op->rest.accept(this);
        }
    }

    void visit(const ProducerConsumer *op) override {
        result = op;
    }

    void visit(const For *op) override {
        if (op->for_type == ForType::GPUBlock) {
            result = op;
        } else {
            op->body.accept(this);
        }
    }

public:
    Stmt result;
};

// Inject the buffer handling code for the inputs and outputs at the
// appropriate site.
class InjectBufferCopiesForInputsAndOutputs : public IRMutator {
    Stmt site;

    // Find all references to external buffers.
    class FindInputsAndOutputs : public IRVisitor {
        using IRVisitor::visit;

        void include(const Parameter &p) {
            if (p.defined()) {
                result.insert(p.name());
            }
        }

        void include(const Buffer<> &b) {
            if (b.defined()) {
                result.insert(b.name());
            }
        }

        void visit(const Variable *op) override {
            include(op->param);
            include(op->image);
        }

        void visit(const Load *op) override {
            include(op->param);
            include(op->image);
            IRVisitor::visit(op);
        }

        void visit(const Store *op) override {
            include(op->param);
            IRVisitor::visit(op);
        }

    public:
        set<string> result;
    };

public:
    using IRMutator::mutate;

    Stmt mutate(const Stmt &s) override {
        if (s.same_as(site)) {
            FindInputsAndOutputs finder;
            s.accept(&finder);
            Stmt new_stmt = s;
            for (const string &buf : finder.result) {
                new_stmt = InjectBufferCopiesForSingleBuffer(buf, true).mutate(new_stmt);
            }
            return new_stmt;
        } else {
            return IRMutator::mutate(s);
        }
    }

    InjectBufferCopiesForInputsAndOutputs(Stmt s)
        : site(s) {
    }
};


// For an Intel FPGA, we use OpenCL runtime with multiple command queues. We enqueue every (non-autorun)
// kernel to a command queue, flush it, and do not wait for it to finish. But after we launch the
// (unique) kernel that output the final results from the FPGA, we should wait for all the kernels, including
// this output kernel, to finish, and set dirty the output buffer, and free the buffers of the other kernels.
// So for Intel FPGAs, we have the followinig two classes to correct the IR:
//    1. GatherAndRemoveFPGABufferActions: gather all the necessary buffer actions, and remove them from the IR
//    2. ApplyFPGABufferActions: append the gathered buffer actions behind the ouptut kernel.
class GatherAndRemoveFPGABufferActions : public IRMutator {
    using IRMutator::visit;

public:
    GatherAndRemoveFPGABufferActions(const vector<string> &_FPGA_kernels) :
        FPGA_kernels(_FPGA_kernels) {
            in_an_FPGA_kernel = false;
    }

public:
    bool is_injected_buffer_action_for_FPGA(const Stmt &stmt) {
        if (in_an_FPGA_kernel) {
            if (stmt.as<LetStmt>() != NULL) {
                const LetStmt *let = stmt.as<LetStmt>();
                const Expr value = let->value;
                if (value.as<Call>() != NULL) {
                    const Call *call = value.as<Call>();
                    if (call->name == "halide_device_free" || call->name == "halide_device_and_host_free") {
                        return true;
                    }
                }
            }
            if (stmt.as<Evaluate>() != NULL) {
                const Evaluate *eval = stmt.as<Evaluate>();
                const Expr value = eval->value;
                if (value.as<Call>() != NULL) {
                    const Call *call = value.as<Call>();
                    if (call->name == Call::buffer_set_device_dirty) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    Expr mutate(const Expr &expr) override {
        return IRMutator::mutate(expr);
    }

    Stmt mutate(const Stmt &stmt) override {
        if (is_injected_buffer_action_for_FPGA(stmt)) {
            if (FPGA_buffer_actions.find(kernel_name) == FPGA_buffer_actions.end()) {
                FPGA_buffer_actions[kernel_name] = call_extern_and_assert("halide_opencl_wait_for_kernels_finish", {});
            }
            FPGA_buffer_actions[kernel_name] = Block::make(FPGA_buffer_actions[kernel_name], stmt);
            debug(4) << "Buffer action for FPGA: ****\n" << stmt << "******\n\n";
            // Effectively remove the buffer action from its current position
            Stmt do_nothing = Evaluate::make(Expr(0));
            return do_nothing;
        }

        if (stmt.as<ProducerConsumer>() != NULL) {
            const ProducerConsumer *pc = stmt.as<ProducerConsumer>();
            if (pc->is_producer && (std::find(FPGA_kernels.begin(), FPGA_kernels.end(), pc->name) != FPGA_kernels.end())) {
                in_an_FPGA_kernel = true;
                kernel_name = pc->name;
            } else {
                in_an_FPGA_kernel = false;
            }
        }

        return IRMutator::mutate(stmt);
    }

public:
    map<string, Stmt>  FPGA_buffer_actions; // All the actions to be put behind the output kernel

private:
    const vector<string> &FPGA_kernels; // Funcs running on an FPGA
    bool  in_an_FPGA_kernel;            // Current part of IR is for an FPGA kernel.
    string kernel_name;
};

class ApplyFPGABufferActions : public IRMutator {
    using IRMutator::visit;

public:
    ApplyFPGABufferActions(const map<string, string> &_names_to_match_for_output_kernel, const map<string, Stmt> &_FPGA_buffer_actions) :
        names_to_match_for_output_kernel(_names_to_match_for_output_kernel), FPGA_buffer_actions(_FPGA_buffer_actions) { }

public:
    Stmt visit(const ProducerConsumer *op) override {
        if (op->is_producer) {
            if (names_to_match_for_output_kernel.find(op->name) != names_to_match_for_output_kernel.end()) {
                // This is the output kernel. Simply append all the buffer actions behind it.
                string output_kernel = names_to_match_for_output_kernel.at(op->name);
                internal_assert(FPGA_buffer_actions.find(output_kernel) != FPGA_buffer_actions.end());
                Stmt new_stmt = ProducerConsumer::make(op->name, true, Block::make(op->body, FPGA_buffer_actions.at(output_kernel)));
                return new_stmt;
            }
        }

        Stmt new_stmt = ProducerConsumer::make(op->name, op->is_producer, mutate(op->body));
        return new_stmt;
    }

private:
    const map<string, string> &names_to_match_for_output_kernel; // We process IR from the outermost to the innermost level. When we encounter IR like
                                                                 // "Produce x ...", and x is one of the given names, this piece of IR
                                                                 // is for the unique output kernel on an FPGA that outputs the final results.
    const map<string, Stmt> &FPGA_buffer_actions;                // All the actions to be put behind the output kernel
};

}  // namespace

vector<string> find_consumers_on_a_place(const vector<string> &all_consumers,  const std::map<string, Function> &env, Place place) {
    vector<string> consumers;
    for (auto c : all_consumers) {
        if (env.at(c).place() == place) {
            consumers.push_back(c);
        }
    }
    return consumers;
}

Stmt move_around_host_dev_buffer_copies(Stmt s, const Target &t, const std::map<string, Function> &env) {
    // In the current IR, right below every FPGA kernel, we may set device dirty for the output buffer of the kernel, if any,
    // and free the input buffers of the kernel, if any. The assumption is that the kernel has been launched and finished.
    // However, this is not the case: for an Intel FPGA, we use OpenCL runtime with multiple command queues; We enqueue every
    // (non-autorun) kernel to a command queue, flush it, and do not wait for it to finish.
    // Therefore, we need correct the IR to be in accordance with what really happens in runtime.
    // Here, after we launch the (unique) kernel that output the final results from the FPGA, we should wait for all the kernels,
    // including this output kernel, to finish, and then set dirty and free buffers. In other words, we move all the FPGA
    // kernels' actions after they finish to be behind that unique output kernel.
    if (t.has_feature(Target::IntelFPGA)) {
        vector<string> FPGA_kernels;
        vector<string> output_kernels;
        map<string, vector<string>> call_graph = build_call_graph(env, true, true);
        map<string, vector<string>> reverse_call_graph = build_reverse_call_graph(call_graph);
        // The output kernel is on the device, and has a single consumer, which is a host func.
        // It is also possible that the output kernel has no consumer, in which case, its output
        // needs to be copied explicitly by the programmer with halide_copy_to_host.
        for (auto r : reverse_call_graph) {
            const Function &f = env.at(r.first);
            if (f.place() == Place::Device) {
                FPGA_kernels.push_back(r.first);
                bool is_output_kernel = false;
                if (r.second.size() == 0) { // no consumer at all
                    is_output_kernel = true;
                } else {
                    // Consumers must be either all on the device, or all on the host.
                    vector<string> host_consumers   = find_consumers_on_a_place(r.second, env, Place::Host);
                    vector<string> device_consumers = find_consumers_on_a_place(r.second, env, Place::Device);
                    user_assert(host_consumers.size() == 0 || device_consumers.size() == 0) << "Device function " << r.first
                            << " has both consumers on the host (" << to_string<string>(host_consumers, true)
                            << " ) and consumers on the device (" << to_string<string>(device_consumers, true)
                            << "). Consumers must be either all on the device, or all on the host. For example, the following is wrong:\n"
                            << "\t Func f(Place::Device), g(Place::Device), h(Place::Host);\n"
                            << "\t f(...) = ...\n"
                            << "\t g(...) = f(...)\n"
                            << "\t h(...) = f(...)\n"
                            << "because f is consumed in both host and device. One may fix the issue like this:\n"
                            << "\t Func fh(Place::Device);\n"
                            << "\t fh(...) = f(...)\n"
                            << "\t h(...) = fh(...)\n"
                            << "Now all the consumers of f are on the device. And fh has a single consumer on the host.\n";
                    if (host_consumers.size() == 1) {
                        is_output_kernel = true;
                    }
                }
                if (is_output_kernel) {
                    // user_assert(output_kernel == "") << "There are two FPGA kernels, each having no consumers or having one consumer on the host: "
                    //         << output_kernel << " and " << r.first << ".\n"
                    //         << "Suggestion: When an FPGA kernel has no consumer, or has one consumer on the host, it is supposed to be an output "
                    //         << "kernel (generating the final results). Currently, we require such a kernel to be unique.\n";
                    output_kernels.push_back(r.first);
                }
            }
        }
        user_assert(FPGA_kernels.empty() || output_kernels.size() > 0) << "No output kernel found for FPGA. An output kernel on FPGA has "
                << "no consumer, or has a single consumer on the  host. One and only one such output kernel is expected.\n";

        if (!FPGA_kernels.empty()) {
            debug(2) << "print buffer act" << s;
            GatherAndRemoveFPGABufferActions gatherer(FPGA_kernels);
            s = gatherer.mutate(s);

            // If several UREs are merged, and the the output function is part of them, these UREs are nestly defined.
            // The one of them that appears at the outermost of the nest should be treated as the output kernel.
            map<string, string> func_to_representative = map_func_to_representative(env);
            map<string, string> names_to_match_for_output_kernel;
            for (size_t i = 0; i < output_kernels.size(); i++) {
                const string &represenative = func_to_representative.at(output_kernels[i]);
                for (auto entry : func_to_representative) {
                    if (entry.second == represenative) {
                        names_to_match_for_output_kernel[entry.first] = output_kernels[i];
                    }
                }
            }
            s = ApplyFPGABufferActions(names_to_match_for_output_kernel, gatherer.FPGA_buffer_actions).mutate(s);
        }
    }

    return s;
}

Stmt inject_host_dev_buffer_copies(Stmt s, const Target &t, const std::map<string, Function> &env) {
    // Handle internal allocations
    s = InjectBufferCopies(t).mutate(s);

    // Handle inputs and outputs
    FindOutermostProduce outermost;
    s.accept(&outermost);
    if (outermost.result.defined()) {
        // If the entire pipeline simplified away, or just dispatches
        // to another pipeline, there may be no outermost produce.
        s = InjectBufferCopiesForInputsAndOutputs(outermost.result).mutate(s);
    }

    // If necessary, ajust the positions where the buffer actions are placed.
    s = move_around_host_dev_buffer_copies(s, t, env);
    return s;
}

}  // namespace Internal
}  // namespace Halide
