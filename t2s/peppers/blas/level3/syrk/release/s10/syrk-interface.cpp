#include "syrk-interface.h"


int MAX_DEVICES = 4;
int NUM_QUEUES_TO_CREATE = 5;
int NUM_KERNELS_TO_CREATE = 5;
cl_int status;
cl_context context = NULL;
cl_command_queue cmdQueue[6]; // extra queue for reading buffer D
cl_device_id devices[4];
int current_kernel = 0;
cl_kernel kernel[5];

const char *kernel_name[] = {
    "kernel_aLoader",
    "kernel_bLoader",
    "kernel_cLoader",
    "kernel_Out",
    "kernel_unloader",
};

#ifdef __cplusplus
extern "C" {
#endif

HALIDE_FUNCTION_ATTRS
int syrk(int32_t _OpA, float _Alpha, float _Beta, struct halide_buffer_t *_A_buffer, struct halide_buffer_t *_C_buffer, struct halide_buffer_t *_deserializer_buffer) {
 void * const _ucon = nullptr;
 uint64_t _386 = (uint64_t)(_deserializer_buffer);
 uint64_t _387 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
 bool _388 = _386 != _387;
 if (!_388)  {
  int32_t _389 = halide_error_buffer_argument_is_null(_ucon, "deserializer");
  return _389;
 }
 uint64_t _390 = (uint64_t)(_C_buffer);
 uint64_t _391 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
 bool _392 = _390 != _391;
 if (!_392)  {
  int32_t _393 = halide_error_buffer_argument_is_null(_ucon, "C");
  return _393;
 }
 uint64_t _394 = (uint64_t)(_A_buffer);
 uint64_t _395 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
 bool _396 = _394 != _395;
 if (!_396)  {
  int32_t _397 = halide_error_buffer_argument_is_null(_ucon, "A");
  return _397;
 }
 void *_398 = _halide_buffer_get_host(_A_buffer);
 void * _A = _398;
 uint32_t _399 = _halide_buffer_get_type(_A_buffer);
 int32_t _400 = _halide_buffer_get_dimensions(_A_buffer);
 int32_t _401 = _halide_buffer_get_min(_A_buffer, 0);
 int32_t _402 = _halide_buffer_get_extent(_A_buffer, 0);
 int32_t _403 = _halide_buffer_get_stride(_A_buffer, 0);
 int32_t _404 = _halide_buffer_get_min(_A_buffer, 1);
 int32_t _405 = _halide_buffer_get_extent(_A_buffer, 1);
 int32_t _406 = _halide_buffer_get_stride(_A_buffer, 1);
 void *_407 = _halide_buffer_get_host(_C_buffer);
 void * _C = _407;
 uint32_t _408 = _halide_buffer_get_type(_C_buffer);
 int32_t _409 = _halide_buffer_get_dimensions(_C_buffer);
 int32_t _410 = _halide_buffer_get_min(_C_buffer, 0);
 int32_t _411 = _halide_buffer_get_extent(_C_buffer, 0);
 int32_t _412 = _halide_buffer_get_stride(_C_buffer, 0);
 int32_t _413 = _halide_buffer_get_min(_C_buffer, 1);
 int32_t _414 = _halide_buffer_get_extent(_C_buffer, 1);
 int32_t _415 = _halide_buffer_get_stride(_C_buffer, 1);
 void *_416 = _halide_buffer_get_host(_deserializer_buffer);
 void * _deserializer = _416;
 uint32_t _417 = _halide_buffer_get_type(_deserializer_buffer);
 int32_t _418 = _halide_buffer_get_dimensions(_deserializer_buffer);
 int32_t _419 = _halide_buffer_get_min(_deserializer_buffer, 0);
 int32_t _420 = _halide_buffer_get_extent(_deserializer_buffer, 0);
 int32_t _421 = _halide_buffer_get_stride(_deserializer_buffer, 0);
 int32_t _422 = _halide_buffer_get_min(_deserializer_buffer, 1);
 int32_t _423 = _halide_buffer_get_extent(_deserializer_buffer, 1);
 int32_t _424 = _halide_buffer_get_stride(_deserializer_buffer, 1);
 int32_t _425 = _halide_buffer_get_min(_deserializer_buffer, 2);
 int32_t _426 = _halide_buffer_get_extent(_deserializer_buffer, 2);
 int32_t _427 = _halide_buffer_get_stride(_deserializer_buffer, 2);
 int32_t _428 = _halide_buffer_get_min(_deserializer_buffer, 3);
 int32_t _429 = _halide_buffer_get_extent(_deserializer_buffer, 3);
 int32_t _430 = _halide_buffer_get_stride(_deserializer_buffer, 3);
 int32_t _431 = _halide_buffer_get_min(_deserializer_buffer, 4);
 int32_t _432 = _halide_buffer_get_extent(_deserializer_buffer, 4);
 int32_t _433 = _halide_buffer_get_stride(_deserializer_buffer, 4);
 int32_t _434 = _halide_buffer_get_min(_deserializer_buffer, 5);
 int32_t _435 = _halide_buffer_get_extent(_deserializer_buffer, 5);
 int32_t _436 = _halide_buffer_get_stride(_deserializer_buffer, 5);
 bool _437 = _halide_buffer_is_bounds_query(_A_buffer);
 if (_437)
 {
  struct halide_dimension_t *_438 = _halide_buffer_get_shape(_A_buffer);
  uint64_t _439 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
  void *_440 = (void *)(_439);
  struct halide_device_interface_t *_441 = (struct halide_device_interface_t *)(_439);
  struct halide_dimension_t s0[2] = {
   {_401, _402, 1, 0},
   {_404, _405, _402, 0},
  };
  struct halide_dimension_t *_442 = s0;
  struct halide_buffer_t *_443 = _halide_buffer_init(_A_buffer, _438, _440, _439, _441, 2, 32, 2, _442, _439);
  (void)_443;
 } // if _437
 bool _444 = _halide_buffer_is_bounds_query(_C_buffer);
 if (_444)
 {
  struct halide_dimension_t *_445 = _halide_buffer_get_shape(_C_buffer);
  uint64_t _446 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
  void *_447 = (void *)(_446);
  struct halide_device_interface_t *_448 = (struct halide_device_interface_t *)(_446);
  struct halide_dimension_t s1[2] = {
   {_410, _411, 1, 0},
   {_413, _414, _411, 0},
  };
  struct halide_dimension_t *_449 = s1;
  struct halide_buffer_t *_450 = _halide_buffer_init(_C_buffer, _445, _447, _446, _448, 2, 32, 2, _449, _446);
  (void)_450;
 } // if _444
 bool _451 = _halide_buffer_is_bounds_query(_deserializer_buffer);
 if (_451)
 {
  struct halide_dimension_t *_452 = _halide_buffer_get_shape(_deserializer_buffer);
  uint64_t _453 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
  void *_454 = (void *)(_453);
  struct halide_device_interface_t *_455 = (struct halide_device_interface_t *)(_453);
  struct halide_dimension_t s2[6] = {
   {0, 12, 1, 0},
   {0, 12, 12, 0},
   {0, 8, 144, 0},
   {0, 8, 1152, 0},
   {0, 9, 9216, 0},
   {0, 4, 82944, 0},
  };
  struct halide_dimension_t *_456 = s2;
  struct halide_buffer_t *_457 = _halide_buffer_init(_deserializer_buffer, _452, _454, _453, _455, 2, 32, 6, _456, _453);
  (void)_457;
 } // if _451
 bool _458 = _halide_buffer_is_bounds_query(_deserializer_buffer);
 bool _459 = _halide_buffer_is_bounds_query(_A_buffer);
 bool _460 = _halide_buffer_is_bounds_query(_C_buffer);
 bool _461 = _459 || _460;
 bool _462 = _458 || _461;
 bool _463 = !(_462);
 if (_463)
 {
  uint32_t _464 = (uint32_t)(ADD_UINT64_T_SUFFIX(73730));
  bool _465 = _399 == _464;
  if (!_465)   {
   uint32_t _466 = (uint32_t)(ADD_UINT64_T_SUFFIX(73730));
   int32_t _467 = halide_error_bad_type(_ucon, "Input buffer A", _399, _466);
   return _467;
  }
  bool _468 = _400 == 2;
  if (!_468)   {
   int32_t _469 = halide_error_bad_dimensions(_ucon, "Input buffer A", _400, 2);
   return _469;
  }
  uint32_t _470 = (uint32_t)(ADD_UINT64_T_SUFFIX(73730));
  bool _471 = _408 == _470;
  if (!_471)   {
   uint32_t _472 = (uint32_t)(ADD_UINT64_T_SUFFIX(73730));
   int32_t _473 = halide_error_bad_type(_ucon, "Input buffer C", _408, _472);
   return _473;
  }
  bool _474 = _409 == 2;
  if (!_474)   {
   int32_t _475 = halide_error_bad_dimensions(_ucon, "Input buffer C", _409, 2);
   return _475;
  }
  uint32_t _476 = (uint32_t)(ADD_UINT64_T_SUFFIX(73730));
  bool _477 = _417 == _476;
  if (!_477)   {
   uint32_t _478 = (uint32_t)(ADD_UINT64_T_SUFFIX(73730));
   int32_t _479 = halide_error_bad_type(_ucon, "Output buffer deserializer", _417, _478);
   return _479;
  }
  bool _480 = _418 == 6;
  if (!_480)   {
   int32_t _481 = halide_error_bad_dimensions(_ucon, "Output buffer deserializer", _418, 6);
   return _481;
  }
  bool _482 = 0 <= _402;
  if (!_482)   {
   int32_t _483 = halide_error_buffer_extents_negative(_ucon, "Input buffer A", 0, _402);
   return _483;
  }
  bool _484 = 0 <= _405;
  if (!_484)   {
   int32_t _485 = halide_error_buffer_extents_negative(_ucon, "Input buffer A", 1, _405);
   return _485;
  }
  bool _486 = 0 <= _411;
  if (!_486)   {
   int32_t _487 = halide_error_buffer_extents_negative(_ucon, "Input buffer C", 0, _411);
   return _487;
  }
  bool _488 = 0 <= _414;
  if (!_488)   {
   int32_t _489 = halide_error_buffer_extents_negative(_ucon, "Input buffer C", 1, _414);
   return _489;
  }
  bool _490 = _419 <= 0;
  int32_t _491 = _420 + _419;
  bool _492 = 12 <= _491;
  bool _493 = _490 && _492;
  if (!_493)   {
   int32_t _494 = _420 + _419;
   int32_t _495 = _494 + -1;
   int32_t _496 = halide_error_access_out_of_bounds(_ucon, "Output buffer deserializer", 0, 0, 11, _419, _495);
   return _496;
  }
  bool _497 = 0 <= _420;
  if (!_497)   {
   int32_t _498 = halide_error_buffer_extents_negative(_ucon, "Output buffer deserializer", 0, _420);
   return _498;
  }
  bool _499 = _422 <= 0;
  int32_t _500 = _423 + _422;
  bool _501 = 12 <= _500;
  bool _502 = _499 && _501;
  if (!_502)   {
   int32_t _503 = _423 + _422;
   int32_t _504 = _503 + -1;
   int32_t _505 = halide_error_access_out_of_bounds(_ucon, "Output buffer deserializer", 1, 0, 11, _422, _504);
   return _505;
  }
  bool _506 = 0 <= _423;
  if (!_506)   {
   int32_t _507 = halide_error_buffer_extents_negative(_ucon, "Output buffer deserializer", 1, _423);
   return _507;
  }
  bool _508 = _425 <= 0;
  int32_t _509 = _426 + _425;
  bool _510 = 8 <= _509;
  bool _511 = _508 && _510;
  if (!_511)   {
   int32_t _512 = _426 + _425;
   int32_t _513 = _512 + -1;
   int32_t _514 = halide_error_access_out_of_bounds(_ucon, "Output buffer deserializer", 2, 0, 7, _425, _513);
   return _514;
  }
  bool _515 = 0 <= _426;
  if (!_515)   {
   int32_t _516 = halide_error_buffer_extents_negative(_ucon, "Output buffer deserializer", 2, _426);
   return _516;
  }
  bool _517 = _428 <= 0;
  int32_t _518 = _429 + _428;
  bool _519 = 8 <= _518;
  bool _520 = _517 && _519;
  if (!_520)   {
   int32_t _521 = _429 + _428;
   int32_t _522 = _521 + -1;
   int32_t _523 = halide_error_access_out_of_bounds(_ucon, "Output buffer deserializer", 3, 0, 7, _428, _522);
   return _523;
  }
  bool _524 = 0 <= _429;
  if (!_524)   {
   int32_t _525 = halide_error_buffer_extents_negative(_ucon, "Output buffer deserializer", 3, _429);
   return _525;
  }
  bool _526 = _431 <= 0;
  int32_t _527 = _432 + _431;
  bool _528 = 9 <= _527;
  bool _529 = _526 && _528;
  if (!_529)   {
   int32_t _530 = _432 + _431;
   int32_t _531 = _530 + -1;
   int32_t _532 = halide_error_access_out_of_bounds(_ucon, "Output buffer deserializer", 4, 0, 8, _431, _531);
   return _532;
  }
  bool _533 = 0 <= _432;
  if (!_533)   {
   int32_t _534 = halide_error_buffer_extents_negative(_ucon, "Output buffer deserializer", 4, _432);
   return _534;
  }
  bool _535 = _434 <= 0;
  int32_t _536 = _435 + _434;
  bool _537 = 4 <= _536;
  bool _538 = _535 && _537;
  if (!_538)   {
   int32_t _539 = _435 + _434;
   int32_t _540 = _539 + -1;
   int32_t _541 = halide_error_access_out_of_bounds(_ucon, "Output buffer deserializer", 5, 0, 3, _434, _540);
   return _541;
  }
  bool _542 = 0 <= _435;
  if (!_542)   {
   int32_t _543 = halide_error_buffer_extents_negative(_ucon, "Output buffer deserializer", 5, _435);
   return _543;
  }
  bool _544 = _403 == 1;
  if (!_544)   {
   int32_t _545 = halide_error_constraint_violated(_ucon, "A.stride.0", _403, "1", 1);
   return _545;
  }
  bool _546 = _412 == 1;
  if (!_546)   {
   int32_t _547 = halide_error_constraint_violated(_ucon, "C.stride.0", _412, "1", 1);
   return _547;
  }
  bool _548 = _421 == 1;
  if (!_548)   {
   int32_t _549 = halide_error_constraint_violated(_ucon, "deserializer.stride.0", _421, "1", 1);
   return _549;
  }
  int64_t _550 = (int64_t)(_405);
  int64_t _551 = (int64_t)(_402);
  int64_t _552 = _550 * _551;
  int64_t _553 = (int64_t)(_414);
  int64_t _554 = (int64_t)(_411);
  int64_t _555 = _553 * _554;
  int64_t _556 = (int64_t)(_423);
  int64_t _557 = (int64_t)(_420);
  int64_t _558 = _556 * _557;
  int64_t _559 = (int64_t)(_426);
  int64_t _560 = _558 * _559;
  int64_t _561 = (int64_t)(_429);
  int64_t _562 = _560 * _561;
  int64_t _563 = (int64_t)(_432);
  int64_t _564 = _562 * _563;
  int64_t _565 = (int64_t)(_435);
  int64_t _566 = _564 * _565;
  int64_t _567;
  int64_t _568 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _569 = _551 > _568;
  if (_569)
  {
   int64_t _570 = (int64_t)(_402);
   _567 = _570;
  } // if _569
  else
  {
   int64_t _571 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _572 = (int64_t)(_402);
   int64_t _573 = _571 - _572;
   _567 = _573;
  } // if _569 else
  int64_t _574 = _567;
  uint64_t _575 = (uint64_t)(_574);
  uint64_t _576 = _575;
  uint64_t _577 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _578 = _576 <= _577;
  if (!_578)   {
   int64_t _579;
   int64_t _580 = (int64_t)(_402);
   int64_t _581 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _582 = _580 > _581;
   if (_582)
   {
    int64_t _583 = (int64_t)(_402);
    _579 = _583;
   } // if _582
   else
   {
    int64_t _584 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _585 = (int64_t)(_402);
    int64_t _586 = _584 - _585;
    _579 = _586;
   } // if _582 else
   int64_t _587 = _579;
   uint64_t _588 = (uint64_t)(_587);
   uint64_t _589 = _588;
   uint64_t _590 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _591 = halide_error_buffer_allocation_too_large(_ucon, "A", _589, _590);
   return _591;
  }
  int64_t _592;
  int64_t _593 = (int64_t)(_405);
  int64_t _594 = (int64_t)(_406);
  int64_t _595 = _593 * _594;
  int64_t _596 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _597 = _595 > _596;
  if (_597)
  {
   int64_t _598 = (int64_t)(_405);
   int64_t _599 = (int64_t)(_406);
   int64_t _600 = _598 * _599;
   _592 = _600;
  } // if _597
  else
  {
   int64_t _601 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _602 = (int64_t)(_405);
   int64_t _603 = (int64_t)(_406);
   int64_t _604 = _602 * _603;
   int64_t _605 = _601 - _604;
   _592 = _605;
  } // if _597 else
  int64_t _606 = _592;
  uint64_t _607 = (uint64_t)(_606);
  uint64_t _608 = _607;
  uint64_t _609 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _610 = _608 <= _609;
  if (!_610)   {
   int64_t _611;
   int64_t _612 = (int64_t)(_405);
   int64_t _613 = (int64_t)(_406);
   int64_t _614 = _612 * _613;
   int64_t _615 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _616 = _614 > _615;
   if (_616)
   {
    int64_t _617 = (int64_t)(_405);
    int64_t _618 = (int64_t)(_406);
    int64_t _619 = _617 * _618;
    _611 = _619;
   } // if _616
   else
   {
    int64_t _620 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _621 = (int64_t)(_405);
    int64_t _622 = (int64_t)(_406);
    int64_t _623 = _621 * _622;
    int64_t _624 = _620 - _623;
    _611 = _624;
   } // if _616 else
   int64_t _625 = _611;
   uint64_t _626 = (uint64_t)(_625);
   uint64_t _627 = _626;
   uint64_t _628 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _629 = halide_error_buffer_allocation_too_large(_ucon, "A", _627, _628);
   return _629;
  }
  int64_t _630 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
  bool _631 = _552 <= _630;
  if (!_631)   {
   int64_t _632 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
   int32_t _633 = halide_error_buffer_extents_too_large(_ucon, "A", _552, _632);
   return _633;
  }
  int64_t _634;
  int64_t _635 = (int64_t)(_411);
  int64_t _636 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _637 = _635 > _636;
  if (_637)
  {
   int64_t _638 = (int64_t)(_411);
   _634 = _638;
  } // if _637
  else
  {
   int64_t _639 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _640 = (int64_t)(_411);
   int64_t _641 = _639 - _640;
   _634 = _641;
  } // if _637 else
  int64_t _642 = _634;
  uint64_t _643 = (uint64_t)(_642);
  uint64_t _644 = _643;
  uint64_t _645 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _646 = _644 <= _645;
  if (!_646)   {
   int64_t _647;
   int64_t _648 = (int64_t)(_411);
   int64_t _649 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _650 = _648 > _649;
   if (_650)
   {
    int64_t _651 = (int64_t)(_411);
    _647 = _651;
   } // if _650
   else
   {
    int64_t _652 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _653 = (int64_t)(_411);
    int64_t _654 = _652 - _653;
    _647 = _654;
   } // if _650 else
   int64_t _655 = _647;
   uint64_t _656 = (uint64_t)(_655);
   uint64_t _657 = _656;
   uint64_t _658 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _659 = halide_error_buffer_allocation_too_large(_ucon, "C", _657, _658);
   return _659;
  }
  int64_t _660;
  int64_t _661 = (int64_t)(_414);
  int64_t _662 = (int64_t)(_415);
  int64_t _663 = _661 * _662;
  int64_t _664 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _665 = _663 > _664;
  if (_665)
  {
   int64_t _666 = (int64_t)(_414);
   int64_t _667 = (int64_t)(_415);
   int64_t _668 = _666 * _667;
   _660 = _668;
  } // if _665
  else
  {
   int64_t _669 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _670 = (int64_t)(_414);
   int64_t _671 = (int64_t)(_415);
   int64_t _672 = _670 * _671;
   int64_t _673 = _669 - _672;
   _660 = _673;
  } // if _665 else
  int64_t _674 = _660;
  uint64_t _675 = (uint64_t)(_674);
  uint64_t _676 = _675;
  uint64_t _677 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _678 = _676 <= _677;
  if (!_678)   {
   int64_t _679;
   int64_t _680 = (int64_t)(_414);
   int64_t _681 = (int64_t)(_415);
   int64_t _682 = _680 * _681;
   int64_t _683 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _684 = _682 > _683;
   if (_684)
   {
    int64_t _685 = (int64_t)(_414);
    int64_t _686 = (int64_t)(_415);
    int64_t _687 = _685 * _686;
    _679 = _687;
   } // if _684
   else
   {
    int64_t _688 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _689 = (int64_t)(_414);
    int64_t _690 = (int64_t)(_415);
    int64_t _691 = _689 * _690;
    int64_t _692 = _688 - _691;
    _679 = _692;
   } // if _684 else
   int64_t _693 = _679;
   uint64_t _694 = (uint64_t)(_693);
   uint64_t _695 = _694;
   uint64_t _696 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _697 = halide_error_buffer_allocation_too_large(_ucon, "C", _695, _696);
   return _697;
  }
  int64_t _698 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
  bool _699 = _555 <= _698;
  if (!_699)   {
   int64_t _700 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
   int32_t _701 = halide_error_buffer_extents_too_large(_ucon, "C", _555, _700);
   return _701;
  }
  int64_t _702;
  int64_t _703 = (int64_t)(_420);
  int64_t _704 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _705 = _703 > _704;
  if (_705)
  {
   int64_t _706 = (int64_t)(_420);
   _702 = _706;
  } // if _705
  else
  {
   int64_t _707 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _708 = (int64_t)(_420);
   int64_t _709 = _707 - _708;
   _702 = _709;
  } // if _705 else
  int64_t _710 = _702;
  uint64_t _711 = (uint64_t)(_710);
  uint64_t _712 = _711;
  uint64_t _713 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _714 = _712 <= _713;
  if (!_714)   {
   int64_t _715;
   int64_t _716 = (int64_t)(_420);
   int64_t _717 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _718 = _716 > _717;
   if (_718)
   {
    int64_t _719 = (int64_t)(_420);
    _715 = _719;
   } // if _718
   else
   {
    int64_t _720 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _721 = (int64_t)(_420);
    int64_t _722 = _720 - _721;
    _715 = _722;
   } // if _718 else
   int64_t _723 = _715;
   uint64_t _724 = (uint64_t)(_723);
   uint64_t _725 = _724;
   uint64_t _726 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _727 = halide_error_buffer_allocation_too_large(_ucon, "deserializer", _725, _726);
   return _727;
  }
  int64_t _728;
  int64_t _729 = (int64_t)(_423);
  int64_t _730 = (int64_t)(_424);
  int64_t _731 = _729 * _730;
  int64_t _732 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _733 = _731 > _732;
  if (_733)
  {
   int64_t _734 = (int64_t)(_423);
   int64_t _735 = (int64_t)(_424);
   int64_t _736 = _734 * _735;
   _728 = _736;
  } // if _733
  else
  {
   int64_t _737 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _738 = (int64_t)(_423);
   int64_t _739 = (int64_t)(_424);
   int64_t _740 = _738 * _739;
   int64_t _741 = _737 - _740;
   _728 = _741;
  } // if _733 else
  int64_t _742 = _728;
  uint64_t _743 = (uint64_t)(_742);
  uint64_t _744 = _743;
  uint64_t _745 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _746 = _744 <= _745;
  if (!_746)   {
   int64_t _747;
   int64_t _748 = (int64_t)(_423);
   int64_t _749 = (int64_t)(_424);
   int64_t _750 = _748 * _749;
   int64_t _751 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _752 = _750 > _751;
   if (_752)
   {
    int64_t _753 = (int64_t)(_423);
    int64_t _754 = (int64_t)(_424);
    int64_t _755 = _753 * _754;
    _747 = _755;
   } // if _752
   else
   {
    int64_t _756 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _757 = (int64_t)(_423);
    int64_t _758 = (int64_t)(_424);
    int64_t _759 = _757 * _758;
    int64_t _760 = _756 - _759;
    _747 = _760;
   } // if _752 else
   int64_t _761 = _747;
   uint64_t _762 = (uint64_t)(_761);
   uint64_t _763 = _762;
   uint64_t _764 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _765 = halide_error_buffer_allocation_too_large(_ucon, "deserializer", _763, _764);
   return _765;
  }
  int64_t _766 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
  bool _767 = _558 <= _766;
  if (!_767)   {
   int64_t _768 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
   int32_t _769 = halide_error_buffer_extents_too_large(_ucon, "deserializer", _558, _768);
   return _769;
  }
  int64_t _770;
  int64_t _771 = (int64_t)(_426);
  int64_t _772 = (int64_t)(_427);
  int64_t _773 = _771 * _772;
  int64_t _774 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _775 = _773 > _774;
  if (_775)
  {
   int64_t _776 = (int64_t)(_426);
   int64_t _777 = (int64_t)(_427);
   int64_t _778 = _776 * _777;
   _770 = _778;
  } // if _775
  else
  {
   int64_t _779 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _780 = (int64_t)(_426);
   int64_t _781 = (int64_t)(_427);
   int64_t _782 = _780 * _781;
   int64_t _783 = _779 - _782;
   _770 = _783;
  } // if _775 else
  int64_t _784 = _770;
  uint64_t _785 = (uint64_t)(_784);
  uint64_t _786 = _785;
  uint64_t _787 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _788 = _786 <= _787;
  if (!_788)   {
   int64_t _789;
   int64_t _790 = (int64_t)(_426);
   int64_t _791 = (int64_t)(_427);
   int64_t _792 = _790 * _791;
   int64_t _793 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _794 = _792 > _793;
   if (_794)
   {
    int64_t _795 = (int64_t)(_426);
    int64_t _796 = (int64_t)(_427);
    int64_t _797 = _795 * _796;
    _789 = _797;
   } // if _794
   else
   {
    int64_t _798 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _799 = (int64_t)(_426);
    int64_t _800 = (int64_t)(_427);
    int64_t _801 = _799 * _800;
    int64_t _802 = _798 - _801;
    _789 = _802;
   } // if _794 else
   int64_t _803 = _789;
   uint64_t _804 = (uint64_t)(_803);
   uint64_t _805 = _804;
   uint64_t _806 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _807 = halide_error_buffer_allocation_too_large(_ucon, "deserializer", _805, _806);
   return _807;
  }
  int64_t _808 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
  bool _809 = _560 <= _808;
  if (!_809)   {
   int64_t _810 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
   int32_t _811 = halide_error_buffer_extents_too_large(_ucon, "deserializer", _560, _810);
   return _811;
  }
  int64_t _812;
  int64_t _813 = (int64_t)(_429);
  int64_t _814 = (int64_t)(_430);
  int64_t _815 = _813 * _814;
  int64_t _816 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _817 = _815 > _816;
  if (_817)
  {
   int64_t _818 = (int64_t)(_429);
   int64_t _819 = (int64_t)(_430);
   int64_t _820 = _818 * _819;
   _812 = _820;
  } // if _817
  else
  {
   int64_t _821 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _822 = (int64_t)(_429);
   int64_t _823 = (int64_t)(_430);
   int64_t _824 = _822 * _823;
   int64_t _825 = _821 - _824;
   _812 = _825;
  } // if _817 else
  int64_t _826 = _812;
  uint64_t _827 = (uint64_t)(_826);
  uint64_t _828 = _827;
  uint64_t _829 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _830 = _828 <= _829;
  if (!_830)   {
   int64_t _831;
   int64_t _832 = (int64_t)(_429);
   int64_t _833 = (int64_t)(_430);
   int64_t _834 = _832 * _833;
   int64_t _835 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _836 = _834 > _835;
   if (_836)
   {
    int64_t _837 = (int64_t)(_429);
    int64_t _838 = (int64_t)(_430);
    int64_t _839 = _837 * _838;
    _831 = _839;
   } // if _836
   else
   {
    int64_t _840 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _841 = (int64_t)(_429);
    int64_t _842 = (int64_t)(_430);
    int64_t _843 = _841 * _842;
    int64_t _844 = _840 - _843;
    _831 = _844;
   } // if _836 else
   int64_t _845 = _831;
   uint64_t _846 = (uint64_t)(_845);
   uint64_t _847 = _846;
   uint64_t _848 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _849 = halide_error_buffer_allocation_too_large(_ucon, "deserializer", _847, _848);
   return _849;
  }
  int64_t _850 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
  bool _851 = _562 <= _850;
  if (!_851)   {
   int64_t _852 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
   int32_t _853 = halide_error_buffer_extents_too_large(_ucon, "deserializer", _562, _852);
   return _853;
  }
  int64_t _854;
  int64_t _855 = (int64_t)(_432);
  int64_t _856 = (int64_t)(_433);
  int64_t _857 = _855 * _856;
  int64_t _858 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _859 = _857 > _858;
  if (_859)
  {
   int64_t _860 = (int64_t)(_432);
   int64_t _861 = (int64_t)(_433);
   int64_t _862 = _860 * _861;
   _854 = _862;
  } // if _859
  else
  {
   int64_t _863 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _864 = (int64_t)(_432);
   int64_t _865 = (int64_t)(_433);
   int64_t _866 = _864 * _865;
   int64_t _867 = _863 - _866;
   _854 = _867;
  } // if _859 else
  int64_t _868 = _854;
  uint64_t _869 = (uint64_t)(_868);
  uint64_t _870 = _869;
  uint64_t _871 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _872 = _870 <= _871;
  if (!_872)   {
   int64_t _873;
   int64_t _874 = (int64_t)(_432);
   int64_t _875 = (int64_t)(_433);
   int64_t _876 = _874 * _875;
   int64_t _877 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _878 = _876 > _877;
   if (_878)
   {
    int64_t _879 = (int64_t)(_432);
    int64_t _880 = (int64_t)(_433);
    int64_t _881 = _879 * _880;
    _873 = _881;
   } // if _878
   else
   {
    int64_t _882 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _883 = (int64_t)(_432);
    int64_t _884 = (int64_t)(_433);
    int64_t _885 = _883 * _884;
    int64_t _886 = _882 - _885;
    _873 = _886;
   } // if _878 else
   int64_t _887 = _873;
   uint64_t _888 = (uint64_t)(_887);
   uint64_t _889 = _888;
   uint64_t _890 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _891 = halide_error_buffer_allocation_too_large(_ucon, "deserializer", _889, _890);
   return _891;
  }
  int64_t _892 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
  bool _893 = _564 <= _892;
  if (!_893)   {
   int64_t _894 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
   int32_t _895 = halide_error_buffer_extents_too_large(_ucon, "deserializer", _564, _894);
   return _895;
  }
  int64_t _896;
  int64_t _897 = (int64_t)(_435);
  int64_t _898 = (int64_t)(_436);
  int64_t _899 = _897 * _898;
  int64_t _900 = (int64_t)(ADD_INT64_T_SUFFIX(0));
  bool _901 = _899 > _900;
  if (_901)
  {
   int64_t _902 = (int64_t)(_435);
   int64_t _903 = (int64_t)(_436);
   int64_t _904 = _902 * _903;
   _896 = _904;
  } // if _901
  else
  {
   int64_t _905 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   int64_t _906 = (int64_t)(_435);
   int64_t _907 = (int64_t)(_436);
   int64_t _908 = _906 * _907;
   int64_t _909 = _905 - _908;
   _896 = _909;
  } // if _901 else
  int64_t _910 = _896;
  uint64_t _911 = (uint64_t)(_910);
  uint64_t _912 = _911;
  uint64_t _913 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
  bool _914 = _912 <= _913;
  if (!_914)   {
   int64_t _915;
   int64_t _916 = (int64_t)(_435);
   int64_t _917 = (int64_t)(_436);
   int64_t _918 = _916 * _917;
   int64_t _919 = (int64_t)(ADD_INT64_T_SUFFIX(0));
   bool _920 = _918 > _919;
   if (_920)
   {
    int64_t _921 = (int64_t)(_435);
    int64_t _922 = (int64_t)(_436);
    int64_t _923 = _921 * _922;
    _915 = _923;
   } // if _920
   else
   {
    int64_t _924 = (int64_t)(ADD_INT64_T_SUFFIX(0));
    int64_t _925 = (int64_t)(_435);
    int64_t _926 = (int64_t)(_436);
    int64_t _927 = _925 * _926;
    int64_t _928 = _924 - _927;
    _915 = _928;
   } // if _920 else
   int64_t _929 = _915;
   uint64_t _930 = (uint64_t)(_929);
   uint64_t _931 = _930;
   uint64_t _932 = (uint64_t)(ADD_UINT64_T_SUFFIX(2147483647));
   int32_t _933 = halide_error_buffer_allocation_too_large(_ucon, "deserializer", _931, _932);
   return _933;
  }
  int64_t _934 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
  bool _935 = _566 <= _934;
  if (!_935)   {
   int64_t _936 = (int64_t)(ADD_INT64_T_SUFFIX(2147483647));
   int32_t _937 = halide_error_buffer_extents_too_large(_ucon, "deserializer", _566, _936);
   return _937;
  }
  uint64_t _938 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
  void *_939 = (void *)(_938);
  bool _940 = _A != _939;
  if (!_940)   {
   int32_t _941 = halide_error_host_is_null(_ucon, "Input buffer A");
   return _941;
  }
  uint64_t _942 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
  void *_943 = (void *)(_942);
  bool _944 = _C != _943;
  if (!_944)   {
   int32_t _945 = halide_error_host_is_null(_ucon, "Input buffer C");
   return _945;
  }
  uint64_t _946 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
  void *_947 = (void *)(_946);
  bool _948 = _deserializer != _947;
  if (!_948)   {
   int32_t _949 = halide_error_host_is_null(_ucon, "Output buffer deserializer");
   return _949;
  }
  struct halide_dimension_t s3[9] = {
   {0, 16, 1, 0},
   {0, 1, 16, 0},
   {0, 12, 16, 0},
   {0, 1, 192, 0},
   {0, 8, 192, 0},
   {0, 8, 1536, 0},
   {0, 8, 12288, 0},
   {0, 9, 98304, 0},
   {0, 4, 884736, 0},
  };
  struct halide_dimension_t *_950 = s3;
  struct halide_dimension_t * _t189 = _950;
  halide_buffer_t b2;
  struct halide_buffer_t *_951 = &b2;
  uint64_t _952 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
  void *_953 = (void *)(_952);
  struct halide_device_interface_t *_954 = (struct halide_device_interface_t *)(_952);
  struct halide_buffer_t *_955 = _halide_buffer_init(_951, _t189, _953, _952, _954, 2, 32, 9, _t189, _952);
  struct halide_buffer_t * _aSerializer_buffer = _955;
  struct halide_device_interface_t const *_956 = halide_opencl_device_interface();
  int32_t _957 = halide_device_and_host_malloc(_ucon, _aSerializer_buffer, _956);
  bool _958 = _957 == 0;
  if (!_958)   {
   return _957;
  }
  struct s4 { void * const ucon; void * const arg; s4(void *ucon, void *a) : ucon(ucon), arg((void *)a) {} ~s4() { halide_device_and_host_free_as_destructor(ucon, arg); } } d0(_ucon, _aSerializer_buffer);
  void *_959 = 0;
  (void)_959;
  {
   void *_960 = _halide_buffer_get_host(_aSerializer_buffer);
   float *_aSerializer = (float *)(_960);
   if (!_aSerializer)
   {
    return halide_error_out_of_memory(_ucon);
   }
   HalideFreeHelper _aSerializer_free(_ucon, _aSerializer, halide_device_host_nop_free);
   // produce aSerializer
   int32_t _961 = halide_copy_to_host(_ucon, _A_buffer);
   bool _962 = _961 == 0;
   if (!_962)    {
    return _961;
   }
   {
    int32_t _addr_temp;
    _addr_temp = 0;
    for (int _aSerializer_s0_i_j_k_kk_ii_iii_kkk = 0; _aSerializer_s0_i_j_k_kk_ii_iii_kkk < 0 + 3538944; _aSerializer_s0_i_j_k_kk_ii_iii_kkk++)
    {
     int32_t _963 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk % 98304;
     int32_t _964 = _963 >> 31;
     int32_t _965;
     bool _966 = 98304 > 0;
     if (_966)
     {
      _965 = 98304;
     } // if _966
     else
     {
      int32_t _967 = 0 - 98304;
      _965 = _967;
     } // if _966 else
     int32_t _968 = _965;
     uint32_t _969 = (uint32_t)(_968);
     uint32_t _970 = _969;
     int32_t _971 = (int32_t)(_970);
     int32_t _972 = _964 & _971;
     int32_t _973 = _963 + _972;
     int32_t _974 = _973 / 12288;
     int32_t _975 = _974 * 12288;
     int32_t _976 = _973 - _975;
     int32_t _977 = _976 >> 31;
     int32_t _978 = 12288 >> 31;
     int32_t _979 = _977 & _978;
     int32_t _980 = _974 - _979;
     int32_t _981 = ~_978;
     int32_t _982 = _977 & _981;
     int32_t _983 = _980 + _982;
     int32_t _984 = _983 * 128;
     int32_t _985 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk % 12288;
     int32_t _986 = _985 >> 31;
     int32_t _987;
     bool _988 = 12288 > 0;
     if (_988)
     {
      _987 = 12288;
     } // if _988
     else
     {
      int32_t _989 = 0 - 12288;
      _987 = _989;
     } // if _988 else
     int32_t _990 = _987;
     uint32_t _991 = (uint32_t)(_990);
     uint32_t _992 = _991;
     int32_t _993 = (int32_t)(_992);
     int32_t _994 = _986 & _993;
     int32_t _995 = _985 + _994;
     int32_t _996 = _995 / 1536;
     int32_t _997 = _996 * 1536;
     int32_t _998 = _995 - _997;
     int32_t _999 = _998 >> 31;
     int32_t _1000 = 1536 >> 31;
     int32_t _1001 = _999 & _1000;
     int32_t _1002 = _996 - _1001;
     int32_t _1003 = ~_1000;
     int32_t _1004 = _999 & _1003;
     int32_t _1005 = _1002 + _1004;
     int32_t _1006 = _1005 * 16;
     int32_t _1007 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk & 15;
     int32_t _1008 = _1006 + _1007;
     int32_t _1009 = _984 + _1008;
     float _1010;
     int32_t _1011 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk / 884736;
     int32_t _1012 = _1011 * 884736;
     int32_t _1013 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk - _1012;
     int32_t _1014 = _1013 >> 31;
     int32_t _1015 = 884736 >> 31;
     int32_t _1016 = _1014 & _1015;
     int32_t _1017 = _1011 - _1016;
     int32_t _1018 = ~_1015;
     int32_t _1019 = _1014 & _1018;
     int32_t _1020 = _1017 + _1019;
     int32_t _1021 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk % 884736;
     int32_t _1022 = _1021 >> 31;
     int32_t _1023;
     bool _1024 = 884736 > 0;
     if (_1024)
     {
      _1023 = 884736;
     } // if _1024
     else
     {
      int32_t _1025 = 0 - 884736;
      _1023 = _1025;
     } // if _1024 else
     int32_t _1026 = _1023;
     uint32_t _1027 = (uint32_t)(_1026);
     uint32_t _1028 = _1027;
     int32_t _1029 = (int32_t)(_1028);
     int32_t _1030 = _1022 & _1029;
     int32_t _1031 = _1021 + _1030;
     int32_t _1032 = _1031 / 98304;
     int32_t _1033 = _1032 * 98304;
     int32_t _1034 = _1031 - _1033;
     int32_t _1035 = _1034 >> 31;
     int32_t _1036 = 98304 >> 31;
     int32_t _1037 = _1035 & _1036;
     int32_t _1038 = _1032 - _1037;
     int32_t _1039 = ~_1036;
     int32_t _1040 = _1035 & _1039;
     int32_t _1041 = _1038 + _1040;
     bool _1042 = _1020 < _1041;
     if (_1042)
     {
      int32_t _1043 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk / 884736;
      int32_t _1044 = _1043 * 884736;
      int32_t _1045 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk - _1044;
      int32_t _1046 = _1045 >> 31;
      int32_t _1047 = 884736 >> 31;
      int32_t _1048 = _1046 & _1047;
      int32_t _1049 = _1043 - _1048;
      int32_t _1050 = ~_1047;
      int32_t _1051 = _1046 & _1050;
      int32_t _1052 = _1049 + _1051;
      int32_t _1053 = _1052 * 96;
      int32_t _1054 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk % 192;
      int32_t _1055 = _1054 >> 31;
      int32_t _1056;
      bool _1057 = 192 > 0;
      if (_1057)
      {
       _1056 = 192;
      } // if _1057
      else
      {
       int32_t _1058 = 0 - 192;
       _1056 = _1058;
      } // if _1057 else
      int32_t _1059 = _1056;
      uint32_t _1060 = (uint32_t)(_1059);
      uint32_t _1061 = _1060;
      int32_t _1062 = (int32_t)(_1061);
      int32_t _1063 = _1055 & _1062;
      int32_t _1064 = _1054 + _1063;
      int32_t _1065 = _1064 >> 4;
      int32_t _1066 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk % 1536;
      int32_t _1067 = _1066 >> 31;
      int32_t _1068;
      bool _1069 = 1536 > 0;
      if (_1069)
      {
       _1068 = 1536;
      } // if _1069
      else
      {
       int32_t _1070 = 0 - 1536;
       _1068 = _1070;
      } // if _1069 else
      int32_t _1071 = _1068;
      uint32_t _1072 = (uint32_t)(_1071);
      uint32_t _1073 = _1072;
      int32_t _1074 = (int32_t)(_1073);
      int32_t _1075 = _1067 & _1074;
      int32_t _1076 = _1066 + _1075;
      int32_t _1077 = _1076 / 192;
      int32_t _1078 = _1077 * 192;
      int32_t _1079 = _1076 - _1078;
      int32_t _1080 = _1079 >> 31;
      int32_t _1081 = 192 >> 31;
      int32_t _1082 = _1080 & _1081;
      int32_t _1083 = _1077 - _1082;
      int32_t _1084 = ~_1081;
      int32_t _1085 = _1080 & _1084;
      int32_t _1086 = _1083 + _1085;
      int32_t _1087 = _1086 * 12;
      int32_t _1088 = _1065 + _1087;
      int32_t _1089 = _1053 + _1088;
      int32_t _1090 = _1089 * _406;
      int32_t _1091 = _1090 + _1009;
      int32_t _1092 = _404 * _406;
      int32_t _1093 = _1092 + _401;
      int32_t _1094 = _1091 - _1093;
      float _1095 = ((const float *)_A)[_1094];
      _1010 = _1095;
     } // if _1042
     else
     {
      int32_t _1096 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk % 192;
      int32_t _1097 = _1096 >> 31;
      int32_t _1098;
      bool _1099 = 192 > 0;
      if (_1099)
      {
       _1098 = 192;
      } // if _1099
      else
      {
       int32_t _1100 = 0 - 192;
       _1098 = _1100;
      } // if _1099 else
      int32_t _1101 = _1098;
      uint32_t _1102 = (uint32_t)(_1101);
      uint32_t _1103 = _1102;
      int32_t _1104 = (int32_t)(_1103);
      int32_t _1105 = _1097 & _1104;
      int32_t _1106 = _1096 + _1105;
      int32_t _1107 = _1106 >> 4;
      int32_t _1108 = 768 - _1107;
      int32_t _1109 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk % 1536;
      int32_t _1110 = _1109 >> 31;
      int32_t _1111;
      bool _1112 = 1536 > 0;
      if (_1112)
      {
       _1111 = 1536;
      } // if _1112
      else
      {
       int32_t _1113 = 0 - 1536;
       _1111 = _1113;
      } // if _1112 else
      int32_t _1114 = _1111;
      uint32_t _1115 = (uint32_t)(_1114);
      uint32_t _1116 = _1115;
      int32_t _1117 = (int32_t)(_1116);
      int32_t _1118 = _1110 & _1117;
      int32_t _1119 = _1109 + _1118;
      int32_t _1120 = _1119 / 192;
      int32_t _1121 = _1120 * 192;
      int32_t _1122 = _1119 - _1121;
      int32_t _1123 = _1122 >> 31;
      int32_t _1124 = 192 >> 31;
      int32_t _1125 = _1123 & _1124;
      int32_t _1126 = _1120 - _1125;
      int32_t _1127 = ~_1124;
      int32_t _1128 = _1123 & _1127;
      int32_t _1129 = _1126 + _1128;
      int32_t _1130 = _1129 * 12;
      int32_t _1131 = _1108 - _1130;
      int32_t _1132 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk / 884736;
      int32_t _1133 = _1132 * 884736;
      int32_t _1134 = _aSerializer_s0_i_j_k_kk_ii_iii_kkk - _1133;
      int32_t _1135 = _1134 >> 31;
      int32_t _1136 = 884736 >> 31;
      int32_t _1137 = _1135 & _1136;
      int32_t _1138 = _1132 - _1137;
      int32_t _1139 = ~_1136;
      int32_t _1140 = _1135 & _1139;
      int32_t _1141 = _1138 + _1140;
      int32_t _1142 = _1141 * 96;
      int32_t _1143 = _1131 - _1142;
      int32_t _1144 = _1143 * _406;
      int32_t _1145 = _1144 + _1009;
      int32_t _1146 = _404 * _406;
      int32_t _1147 = _1146 + _401;
      int32_t _1148 = _1145 - _1147;
      int32_t _1149 = _1148 - _406;
      float _1150 = ((const float *)_A)[_1149];
      _1010 = _1150;
     } // if _1042 else
     float _1151 = _1010;
     int32_t _1152 = _addr_temp;
     _aSerializer[_1152] = _1151;
     int32_t _1153 = _addr_temp;
     int32_t _1154 = _1153 + 1;
     _addr_temp = _1154;
    } // for _aSerializer_s0_i_j_k_kk_ii_iii_kkk
   } // alloc _addr_temp
   bool _1155 = (bool)(ADD_UINT64_T_SUFFIX(1));
   int32_t _1156 = _halide_buffer_set_host_dirty(_aSerializer_buffer, _1155);
   (void)_1156;
   // consume aSerializer
   // produce aLoader
   struct halide_device_interface_t const *_1157 = halide_opencl_device_interface();
   int32_t _1158 = halide_copy_to_device(_ucon, _aSerializer_buffer, _1157);
   bool _1159 = _1158 == 0;
   if (!_1159)    {
    return _1158;
   }
   status = clSetKernelArg(kernel[current_kernel], 0, sizeof(cl_mem), (void *)&((device_handle *)_halide_buffer_get_device(_aSerializer_buffer))->mem);
   CHECK(status);
   current_kernel++;

   // consume aLoader
   // produce aFeeder
   // consume aFeeder
   struct halide_dimension_t s5[9] = {
    {0, 16, 1, 0},
    {0, 12, 16, 0},
    {0, 1, 192, 0},
    {0, 8, 192, 0},
    {0, 1, 1536, 0},
    {0, 8, 1536, 0},
    {0, 8, 12288, 0},
    {0, 9, 98304, 0},
    {0, 4, 884736, 0},
   };
   struct halide_dimension_t *_1160 = s5;
   struct halide_dimension_t * _t192 = _1160;
   halide_buffer_t b3;
   struct halide_buffer_t *_1161 = &b3;
   uint64_t _1162 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
   void *_1163 = (void *)(_1162);
   struct halide_device_interface_t *_1164 = (struct halide_device_interface_t *)(_1162);
   struct halide_buffer_t *_1165 = _halide_buffer_init(_1161, _t192, _1163, _1162, _1164, 2, 32, 9, _t192, _1162);
   struct halide_buffer_t * _bSerializer_buffer = _1165;
   struct halide_device_interface_t const *_1166 = halide_opencl_device_interface();
   int32_t _1167 = halide_device_and_host_malloc(_ucon, _bSerializer_buffer, _1166);
   bool _1168 = _1167 == 0;
   if (!_1168)    {
    return _1167;
   }
   struct s6 { void * const ucon; void * const arg; s6(void *ucon, void *a) : ucon(ucon), arg((void *)a) {} ~s6() { halide_device_and_host_free_as_destructor(ucon, arg); } } d1(_ucon, _bSerializer_buffer);
   void *_1169 = 0;
   (void)_1169;
   {
    void *_1170 = _halide_buffer_get_host(_bSerializer_buffer);
    float *_bSerializer = (float *)(_1170);
    if (!_bSerializer)
    {
     return halide_error_out_of_memory(_ucon);
    }
    HalideFreeHelper _bSerializer_free(_ucon, _bSerializer, halide_device_host_nop_free);
    // produce bSerializer
    {
     int32_t _addr_temp;
     _addr_temp = 0;
     for (int _bSerializer_s0_i_j_k_kk_jj_jjj_kkk = 0; _bSerializer_s0_i_j_k_kk_jj_jjj_kkk < 0 + 3538944; _bSerializer_s0_i_j_k_kk_jj_jjj_kkk++)
     {
      int32_t _1171 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 98304;
      int32_t _1172 = _1171 >> 31;
      int32_t _1173;
      bool _1174 = 98304 > 0;
      if (_1174)
      {
       _1173 = 98304;
      } // if _1174
      else
      {
       int32_t _1175 = 0 - 98304;
       _1173 = _1175;
      } // if _1174 else
      int32_t _1176 = _1173;
      uint32_t _1177 = (uint32_t)(_1176);
      uint32_t _1178 = _1177;
      int32_t _1179 = (int32_t)(_1178);
      int32_t _1180 = _1172 & _1179;
      int32_t _1181 = _1171 + _1180;
      int32_t _1182 = _1181 / 12288;
      int32_t _1183 = _1182 * 12288;
      int32_t _1184 = _1181 - _1183;
      int32_t _1185 = _1184 >> 31;
      int32_t _1186 = 12288 >> 31;
      int32_t _1187 = _1185 & _1186;
      int32_t _1188 = _1182 - _1187;
      int32_t _1189 = ~_1186;
      int32_t _1190 = _1185 & _1189;
      int32_t _1191 = _1188 + _1190;
      int32_t _1192 = _1191 * 128;
      int32_t _1193 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 12288;
      int32_t _1194 = _1193 >> 31;
      int32_t _1195;
      bool _1196 = 12288 > 0;
      if (_1196)
      {
       _1195 = 12288;
      } // if _1196
      else
      {
       int32_t _1197 = 0 - 12288;
       _1195 = _1197;
      } // if _1196 else
      int32_t _1198 = _1195;
      uint32_t _1199 = (uint32_t)(_1198);
      uint32_t _1200 = _1199;
      int32_t _1201 = (int32_t)(_1200);
      int32_t _1202 = _1194 & _1201;
      int32_t _1203 = _1193 + _1202;
      int32_t _1204 = _1203 / 1536;
      int32_t _1205 = _1204 * 1536;
      int32_t _1206 = _1203 - _1205;
      int32_t _1207 = _1206 >> 31;
      int32_t _1208 = 1536 >> 31;
      int32_t _1209 = _1207 & _1208;
      int32_t _1210 = _1204 - _1209;
      int32_t _1211 = ~_1208;
      int32_t _1212 = _1207 & _1211;
      int32_t _1213 = _1210 + _1212;
      int32_t _1214 = _1213 * 16;
      int32_t _1215 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk & 15;
      int32_t _1216 = _1214 + _1215;
      int32_t _1217 = _1192 + _1216;
      float _1218;
      int32_t _1219 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk / 884736;
      int32_t _1220 = _1219 * 884736;
      int32_t _1221 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk - _1220;
      int32_t _1222 = _1221 >> 31;
      int32_t _1223 = 884736 >> 31;
      int32_t _1224 = _1222 & _1223;
      int32_t _1225 = _1219 - _1224;
      int32_t _1226 = ~_1223;
      int32_t _1227 = _1222 & _1226;
      int32_t _1228 = _1225 + _1227;
      int32_t _1229 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 884736;
      int32_t _1230 = _1229 >> 31;
      int32_t _1231;
      bool _1232 = 884736 > 0;
      if (_1232)
      {
       _1231 = 884736;
      } // if _1232
      else
      {
       int32_t _1233 = 0 - 884736;
       _1231 = _1233;
      } // if _1232 else
      int32_t _1234 = _1231;
      uint32_t _1235 = (uint32_t)(_1234);
      uint32_t _1236 = _1235;
      int32_t _1237 = (int32_t)(_1236);
      int32_t _1238 = _1230 & _1237;
      int32_t _1239 = _1229 + _1238;
      int32_t _1240 = _1239 / 98304;
      int32_t _1241 = _1240 * 98304;
      int32_t _1242 = _1239 - _1241;
      int32_t _1243 = _1242 >> 31;
      int32_t _1244 = 98304 >> 31;
      int32_t _1245 = _1243 & _1244;
      int32_t _1246 = _1240 - _1245;
      int32_t _1247 = ~_1244;
      int32_t _1248 = _1243 & _1247;
      int32_t _1249 = _1246 + _1248;
      bool _1250 = _1228 < _1249;
      if (_1250)
      {
       int32_t _1251 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 884736;
       int32_t _1252 = _1251 >> 31;
       int32_t _1253;
       bool _1254 = 884736 > 0;
       if (_1254)
       {
        _1253 = 884736;
       } // if _1254
       else
       {
        int32_t _1255 = 0 - 884736;
        _1253 = _1255;
       } // if _1254 else
       int32_t _1256 = _1253;
       uint32_t _1257 = (uint32_t)(_1256);
       uint32_t _1258 = _1257;
       int32_t _1259 = (int32_t)(_1258);
       int32_t _1260 = _1252 & _1259;
       int32_t _1261 = _1251 + _1260;
       int32_t _1262 = _1261 / 98304;
       int32_t _1263 = _1262 * 98304;
       int32_t _1264 = _1261 - _1263;
       int32_t _1265 = _1264 >> 31;
       int32_t _1266 = 98304 >> 31;
       int32_t _1267 = _1265 & _1266;
       int32_t _1268 = _1262 - _1267;
       int32_t _1269 = ~_1266;
       int32_t _1270 = _1265 & _1269;
       int32_t _1271 = _1268 + _1270;
       int32_t _1272 = _1271 * 96;
       int32_t _1273 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 192;
       int32_t _1274 = _1273 >> 31;
       int32_t _1275;
       bool _1276 = 192 > 0;
       if (_1276)
       {
        _1275 = 192;
       } // if _1276
       else
       {
        int32_t _1277 = 0 - 192;
        _1275 = _1277;
       } // if _1276 else
       int32_t _1278 = _1275;
       uint32_t _1279 = (uint32_t)(_1278);
       uint32_t _1280 = _1279;
       int32_t _1281 = (int32_t)(_1280);
       int32_t _1282 = _1274 & _1281;
       int32_t _1283 = _1273 + _1282;
       int32_t _1284 = _1283 >> 4;
       int32_t _1285 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 1536;
       int32_t _1286 = _1285 >> 31;
       int32_t _1287;
       bool _1288 = 1536 > 0;
       if (_1288)
       {
        _1287 = 1536;
       } // if _1288
       else
       {
        int32_t _1289 = 0 - 1536;
        _1287 = _1289;
       } // if _1288 else
       int32_t _1290 = _1287;
       uint32_t _1291 = (uint32_t)(_1290);
       uint32_t _1292 = _1291;
       int32_t _1293 = (int32_t)(_1292);
       int32_t _1294 = _1286 & _1293;
       int32_t _1295 = _1285 + _1294;
       int32_t _1296 = _1295 / 192;
       int32_t _1297 = _1296 * 192;
       int32_t _1298 = _1295 - _1297;
       int32_t _1299 = _1298 >> 31;
       int32_t _1300 = 192 >> 31;
       int32_t _1301 = _1299 & _1300;
       int32_t _1302 = _1296 - _1301;
       int32_t _1303 = ~_1300;
       int32_t _1304 = _1299 & _1303;
       int32_t _1305 = _1302 + _1304;
       int32_t _1306 = _1305 * 12;
       int32_t _1307 = _1284 + _1306;
       int32_t _1308 = _1272 + _1307;
       int32_t _1309 = _1308 * _406;
       int32_t _1310 = _1309 + _1217;
       int32_t _1311 = _404 * _406;
       int32_t _1312 = _1311 + _401;
       int32_t _1313 = _1310 - _1312;
       int32_t _1314 = _406 * 96;
       int32_t _1315 = _1313 - _1314;
       float _1316 = ((const float *)_A)[_1315];
       _1218 = _1316;
      } // if _1250
      else
      {
       int32_t _1317 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 192;
       int32_t _1318 = _1317 >> 31;
       int32_t _1319;
       bool _1320 = 192 > 0;
       if (_1320)
       {
        _1319 = 192;
       } // if _1320
       else
       {
        int32_t _1321 = 0 - 192;
        _1319 = _1321;
       } // if _1320 else
       int32_t _1322 = _1319;
       uint32_t _1323 = (uint32_t)(_1322);
       uint32_t _1324 = _1323;
       int32_t _1325 = (int32_t)(_1324);
       int32_t _1326 = _1318 & _1325;
       int32_t _1327 = _1317 + _1326;
       int32_t _1328 = _1327 >> 4;
       int32_t _1329 = 768 - _1328;
       int32_t _1330 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 1536;
       int32_t _1331 = _1330 >> 31;
       int32_t _1332;
       bool _1333 = 1536 > 0;
       if (_1333)
       {
        _1332 = 1536;
       } // if _1333
       else
       {
        int32_t _1334 = 0 - 1536;
        _1332 = _1334;
       } // if _1333 else
       int32_t _1335 = _1332;
       uint32_t _1336 = (uint32_t)(_1335);
       uint32_t _1337 = _1336;
       int32_t _1338 = (int32_t)(_1337);
       int32_t _1339 = _1331 & _1338;
       int32_t _1340 = _1330 + _1339;
       int32_t _1341 = _1340 / 192;
       int32_t _1342 = _1341 * 192;
       int32_t _1343 = _1340 - _1342;
       int32_t _1344 = _1343 >> 31;
       int32_t _1345 = 192 >> 31;
       int32_t _1346 = _1344 & _1345;
       int32_t _1347 = _1341 - _1346;
       int32_t _1348 = ~_1345;
       int32_t _1349 = _1344 & _1348;
       int32_t _1350 = _1347 + _1349;
       int32_t _1351 = _1350 * 12;
       int32_t _1352 = _1329 - _1351;
       int32_t _1353 = _bSerializer_s0_i_j_k_kk_jj_jjj_kkk % 884736;
       int32_t _1354 = _1353 >> 31;
       int32_t _1355;
       bool _1356 = 884736 > 0;
       if (_1356)
       {
        _1355 = 884736;
       } // if _1356
       else
       {
        int32_t _1357 = 0 - 884736;
        _1355 = _1357;
       } // if _1356 else
       int32_t _1358 = _1355;
       uint32_t _1359 = (uint32_t)(_1358);
       uint32_t _1360 = _1359;
       int32_t _1361 = (int32_t)(_1360);
       int32_t _1362 = _1354 & _1361;
       int32_t _1363 = _1353 + _1362;
       int32_t _1364 = _1363 / 98304;
       int32_t _1365 = _1364 * 98304;
       int32_t _1366 = _1363 - _1365;
       int32_t _1367 = _1366 >> 31;
       int32_t _1368 = 98304 >> 31;
       int32_t _1369 = _1367 & _1368;
       int32_t _1370 = _1364 - _1369;
       int32_t _1371 = ~_1368;
       int32_t _1372 = _1367 & _1371;
       int32_t _1373 = _1370 + _1372;
       int32_t _1374 = _1373 * 96;
       int32_t _1375 = _1352 - _1374;
       int32_t _1376 = _1375 * _406;
       int32_t _1377 = _1376 + _1217;
       int32_t _1378 = _404 * _406;
       int32_t _1379 = _1378 + _401;
       int32_t _1380 = _1377 - _1379;
       int32_t _1381 = _1380 - _406;
       float _1382 = ((const float *)_A)[_1381];
       _1218 = _1382;
      } // if _1250 else
      float _1383 = _1218;
      int32_t _1384 = _addr_temp;
      _bSerializer[_1384] = _1383;
      int32_t _1385 = _addr_temp;
      int32_t _1386 = _1385 + 1;
      _addr_temp = _1386;
     } // for _bSerializer_s0_i_j_k_kk_jj_jjj_kkk
    } // alloc _addr_temp
    bool _1387 = (bool)(ADD_UINT64_T_SUFFIX(1));
    int32_t _1388 = _halide_buffer_set_host_dirty(_bSerializer_buffer, _1387);
    (void)_1388;
    // consume bSerializer
    // produce bLoader
    struct halide_device_interface_t const *_1389 = halide_opencl_device_interface();
    int32_t _1390 = halide_copy_to_device(_ucon, _bSerializer_buffer, _1389);
    bool _1391 = _1390 == 0;
    if (!_1391)     {
     return _1390;
    }
    status = clSetKernelArg(kernel[current_kernel], 0, sizeof(cl_mem), (void *)&((device_handle *)_halide_buffer_get_device(_bSerializer_buffer))->mem);
    CHECK(status);
    current_kernel++;

    // consume bLoader
    // produce bFeeder
    // consume bFeeder
    // produce V
    // consume V
    // consume Z
    // consume uB
    // consume uA
    struct halide_dimension_t s7[6] = {
     {0, 12, 1, 0},
     {0, 12, 12, 0},
     {0, 8, 144, 0},
     {0, 8, 1152, 0},
     {0, 9, 9216, 0},
     {0, 4, 82944, 0},
    };
    struct halide_dimension_t *_1392 = s7;
    struct halide_dimension_t * _t195 = _1392;
    halide_buffer_t b4;
    struct halide_buffer_t *_1393 = &b4;
    uint64_t _1394 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
    void *_1395 = (void *)(_1394);
    struct halide_device_interface_t *_1396 = (struct halide_device_interface_t *)(_1394);
    struct halide_buffer_t *_1397 = _halide_buffer_init(_1393, _t195, _1395, _1394, _1396, 2, 32, 6, _t195, _1394);
    struct halide_buffer_t * _cSerializer_buffer = _1397;
    struct halide_device_interface_t const *_1398 = halide_opencl_device_interface();
    int32_t _1399 = halide_device_and_host_malloc(_ucon, _cSerializer_buffer, _1398);
    bool _1400 = _1399 == 0;
    if (!_1400)     {
     return _1399;
    }
    struct s8 { void * const ucon; void * const arg; s8(void *ucon, void *a) : ucon(ucon), arg((void *)a) {} ~s8() { halide_device_and_host_free_as_destructor(ucon, arg); } } d2(_ucon, _cSerializer_buffer);
    void *_1401 = 0;
    (void)_1401;
    {
     void *_1402 = _halide_buffer_get_host(_cSerializer_buffer);
     float *_cSerializer = (float *)(_1402);
     if (!_cSerializer)
     {
      return halide_error_out_of_memory(_ucon);
     }
     HalideFreeHelper _cSerializer_free(_ucon, _cSerializer, halide_device_host_nop_free);
     // produce cSerializer
     int32_t _1403 = halide_copy_to_host(_ucon, _C_buffer);
     bool _1404 = _1403 == 0;
     if (!_1404)      {
      return _1403;
     }
     {
      int32_t _addr_temp;
      _addr_temp = 0;
      for (int _cSerializer_s0_i_j_ii_jj_iii_jjj = 0; _cSerializer_s0_i_j_ii_jj_iii_jjj < 0 + 331776; _cSerializer_s0_i_j_ii_jj_iii_jjj++)
      {
       float _1405;
       int32_t _1406 = _cSerializer_s0_i_j_ii_jj_iii_jjj / 82944;
       int32_t _1407 = _1406 * 82944;
       int32_t _1408 = _cSerializer_s0_i_j_ii_jj_iii_jjj - _1407;
       int32_t _1409 = _1408 >> 31;
       int32_t _1410 = 82944 >> 31;
       int32_t _1411 = _1409 & _1410;
       int32_t _1412 = _1406 - _1411;
       int32_t _1413 = ~_1410;
       int32_t _1414 = _1409 & _1413;
       int32_t _1415 = _1412 + _1414;
       int32_t _1416 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 82944;
       int32_t _1417 = _1416 >> 31;
       int32_t _1418;
       bool _1419 = 82944 > 0;
       if (_1419)
       {
        _1418 = 82944;
       } // if _1419
       else
       {
        int32_t _1420 = 0 - 82944;
        _1418 = _1420;
       } // if _1419 else
       int32_t _1421 = _1418;
       uint32_t _1422 = (uint32_t)(_1421);
       uint32_t _1423 = _1422;
       int32_t _1424 = (int32_t)(_1423);
       int32_t _1425 = _1417 & _1424;
       int32_t _1426 = _1416 + _1425;
       int32_t _1427 = _1426 / 9216;
       int32_t _1428 = _1427 * 9216;
       int32_t _1429 = _1426 - _1428;
       int32_t _1430 = _1429 >> 31;
       int32_t _1431 = 9216 >> 31;
       int32_t _1432 = _1430 & _1431;
       int32_t _1433 = _1427 - _1432;
       int32_t _1434 = ~_1431;
       int32_t _1435 = _1430 & _1434;
       int32_t _1436 = _1433 + _1435;
       bool _1437 = _1415 < _1436;
       if (_1437)
       {
        int32_t _1438 = _cSerializer_s0_i_j_ii_jj_iii_jjj / 82944;
        int32_t _1439 = _1438 * 82944;
        int32_t _1440 = _cSerializer_s0_i_j_ii_jj_iii_jjj - _1439;
        int32_t _1441 = _1440 >> 31;
        int32_t _1442 = 82944 >> 31;
        int32_t _1443 = _1441 & _1442;
        int32_t _1444 = _1438 - _1443;
        int32_t _1445 = ~_1442;
        int32_t _1446 = _1441 & _1445;
        int32_t _1447 = _1444 + _1446;
        int32_t _1448 = _1447 * 96;
        int32_t _1449 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 144;
        int32_t _1450 = _1449 >> 31;
        int32_t _1451;
        bool _1452 = 144 > 0;
        if (_1452)
        {
         _1451 = 144;
        } // if _1452
        else
        {
         int32_t _1453 = 0 - 144;
         _1451 = _1453;
        } // if _1452 else
        int32_t _1454 = _1451;
        uint32_t _1455 = (uint32_t)(_1454);
        uint32_t _1456 = _1455;
        int32_t _1457 = (int32_t)(_1456);
        int32_t _1458 = _1450 & _1457;
        int32_t _1459 = _1449 + _1458;
        int32_t _1460 = _1459 / 12;
        int32_t _1461 = _1460 * 12;
        int32_t _1462 = _1459 - _1461;
        int32_t _1463 = _1462 >> 31;
        int32_t _1464 = 12 >> 31;
        int32_t _1465 = _1463 & _1464;
        int32_t _1466 = _1460 - _1465;
        int32_t _1467 = ~_1464;
        int32_t _1468 = _1463 & _1467;
        int32_t _1469 = _1466 + _1468;
        int32_t _1470 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 9216;
        int32_t _1471 = _1470 >> 31;
        int32_t _1472;
        bool _1473 = 9216 > 0;
        if (_1473)
        {
         _1472 = 9216;
        } // if _1473
        else
        {
         int32_t _1474 = 0 - 9216;
         _1472 = _1474;
        } // if _1473 else
        int32_t _1475 = _1472;
        uint32_t _1476 = (uint32_t)(_1475);
        uint32_t _1477 = _1476;
        int32_t _1478 = (int32_t)(_1477);
        int32_t _1479 = _1471 & _1478;
        int32_t _1480 = _1470 + _1479;
        int32_t _1481 = _1480 / 1152;
        int32_t _1482 = _1481 * 1152;
        int32_t _1483 = _1480 - _1482;
        int32_t _1484 = _1483 >> 31;
        int32_t _1485 = 1152 >> 31;
        int32_t _1486 = _1484 & _1485;
        int32_t _1487 = _1481 - _1486;
        int32_t _1488 = ~_1485;
        int32_t _1489 = _1484 & _1488;
        int32_t _1490 = _1487 + _1489;
        int32_t _1491 = _1490 * 12;
        int32_t _1492 = _1469 + _1491;
        int32_t _1493 = _1448 + _1492;
        int32_t _1494 = _1493 * _415;
        int32_t _1495 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 82944;
        int32_t _1496 = _1495 >> 31;
        int32_t _1497;
        bool _1498 = 82944 > 0;
        if (_1498)
        {
         _1497 = 82944;
        } // if _1498
        else
        {
         int32_t _1499 = 0 - 82944;
         _1497 = _1499;
        } // if _1498 else
        int32_t _1500 = _1497;
        uint32_t _1501 = (uint32_t)(_1500);
        uint32_t _1502 = _1501;
        int32_t _1503 = (int32_t)(_1502);
        int32_t _1504 = _1496 & _1503;
        int32_t _1505 = _1495 + _1504;
        int32_t _1506 = _1505 / 9216;
        int32_t _1507 = _1506 * 9216;
        int32_t _1508 = _1505 - _1507;
        int32_t _1509 = _1508 >> 31;
        int32_t _1510 = 9216 >> 31;
        int32_t _1511 = _1509 & _1510;
        int32_t _1512 = _1506 - _1511;
        int32_t _1513 = ~_1510;
        int32_t _1514 = _1509 & _1513;
        int32_t _1515 = _1512 + _1514;
        int32_t _1516 = _1515 * 96;
        int32_t _1517 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 1152;
        int32_t _1518 = _1517 >> 31;
        int32_t _1519;
        bool _1520 = 1152 > 0;
        if (_1520)
        {
         _1519 = 1152;
        } // if _1520
        else
        {
         int32_t _1521 = 0 - 1152;
         _1519 = _1521;
        } // if _1520 else
        int32_t _1522 = _1519;
        uint32_t _1523 = (uint32_t)(_1522);
        uint32_t _1524 = _1523;
        int32_t _1525 = (int32_t)(_1524);
        int32_t _1526 = _1518 & _1525;
        int32_t _1527 = _1517 + _1526;
        int32_t _1528 = _1527 / 144;
        int32_t _1529 = _1528 * 144;
        int32_t _1530 = _1527 - _1529;
        int32_t _1531 = _1530 >> 31;
        int32_t _1532 = 144 >> 31;
        int32_t _1533 = _1531 & _1532;
        int32_t _1534 = _1528 - _1533;
        int32_t _1535 = ~_1532;
        int32_t _1536 = _1531 & _1535;
        int32_t _1537 = _1534 + _1536;
        int32_t _1538 = _1537 * 12;
        int32_t _1539 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 12;
        int32_t _1540 = _1539 >> 31;
        int32_t _1541;
        bool _1542 = 12 > 0;
        if (_1542)
        {
         _1541 = 12;
        } // if _1542
        else
        {
         int32_t _1543 = 0 - 12;
         _1541 = _1543;
        } // if _1542 else
        int32_t _1544 = _1541;
        uint32_t _1545 = (uint32_t)(_1544);
        uint32_t _1546 = _1545;
        int32_t _1547 = (int32_t)(_1546);
        int32_t _1548 = _1540 & _1547;
        int32_t _1549 = _1539 + _1548;
        int32_t _1550 = _1538 + _1549;
        int32_t _1551 = _1516 + _1550;
        int32_t _1552 = _1494 + _1551;
        int32_t _1553 = _413 * _415;
        int32_t _1554 = _1553 + _410;
        int32_t _1555 = _1552 - _1554;
        int32_t _1556 = _1555 + -96;
        float _1557 = ((const float *)_C)[_1556];
        _1405 = _1557;
       } // if _1437
       else
       {
        int32_t _1558 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 144;
        int32_t _1559 = _1558 >> 31;
        int32_t _1560;
        bool _1561 = 144 > 0;
        if (_1561)
        {
         _1560 = 144;
        } // if _1561
        else
        {
         int32_t _1562 = 0 - 144;
         _1560 = _1562;
        } // if _1561 else
        int32_t _1563 = _1560;
        uint32_t _1564 = (uint32_t)(_1563);
        uint32_t _1565 = _1564;
        int32_t _1566 = (int32_t)(_1565);
        int32_t _1567 = _1559 & _1566;
        int32_t _1568 = _1558 + _1567;
        int32_t _1569 = _1568 / 12;
        int32_t _1570 = _1569 * 12;
        int32_t _1571 = _1568 - _1570;
        int32_t _1572 = _1571 >> 31;
        int32_t _1573 = 12 >> 31;
        int32_t _1574 = _1572 & _1573;
        int32_t _1575 = _1569 - _1574;
        int32_t _1576 = ~_1573;
        int32_t _1577 = _1572 & _1576;
        int32_t _1578 = _1575 + _1577;
        int32_t _1579 = 768 - _1578;
        int32_t _1580 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 9216;
        int32_t _1581 = _1580 >> 31;
        int32_t _1582;
        bool _1583 = 9216 > 0;
        if (_1583)
        {
         _1582 = 9216;
        } // if _1583
        else
        {
         int32_t _1584 = 0 - 9216;
         _1582 = _1584;
        } // if _1583 else
        int32_t _1585 = _1582;
        uint32_t _1586 = (uint32_t)(_1585);
        uint32_t _1587 = _1586;
        int32_t _1588 = (int32_t)(_1587);
        int32_t _1589 = _1581 & _1588;
        int32_t _1590 = _1580 + _1589;
        int32_t _1591 = _1590 / 1152;
        int32_t _1592 = _1591 * 1152;
        int32_t _1593 = _1590 - _1592;
        int32_t _1594 = _1593 >> 31;
        int32_t _1595 = 1152 >> 31;
        int32_t _1596 = _1594 & _1595;
        int32_t _1597 = _1591 - _1596;
        int32_t _1598 = ~_1595;
        int32_t _1599 = _1594 & _1598;
        int32_t _1600 = _1597 + _1599;
        int32_t _1601 = _1600 * 12;
        int32_t _1602 = _1579 - _1601;
        int32_t _1603 = _cSerializer_s0_i_j_ii_jj_iii_jjj / 82944;
        int32_t _1604 = _1603 * 82944;
        int32_t _1605 = _cSerializer_s0_i_j_ii_jj_iii_jjj - _1604;
        int32_t _1606 = _1605 >> 31;
        int32_t _1607 = 82944 >> 31;
        int32_t _1608 = _1606 & _1607;
        int32_t _1609 = _1603 - _1608;
        int32_t _1610 = ~_1607;
        int32_t _1611 = _1606 & _1610;
        int32_t _1612 = _1609 + _1611;
        int32_t _1613 = _1612 * 96;
        int32_t _1614 = _1602 - _1613;
        int32_t _1615 = _1614 * _415;
        int32_t _1616 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 12;
        int32_t _1617 = _1616 >> 31;
        int32_t _1618;
        bool _1619 = 12 > 0;
        if (_1619)
        {
         _1618 = 12;
        } // if _1619
        else
        {
         int32_t _1620 = 0 - 12;
         _1618 = _1620;
        } // if _1619 else
        int32_t _1621 = _1618;
        uint32_t _1622 = (uint32_t)(_1621);
        uint32_t _1623 = _1622;
        int32_t _1624 = (int32_t)(_1623);
        int32_t _1625 = _1617 & _1624;
        int32_t _1626 = _1616 + _1625;
        int32_t _1627 = 768 - _1626;
        int32_t _1628 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 1152;
        int32_t _1629 = _1628 >> 31;
        int32_t _1630;
        bool _1631 = 1152 > 0;
        if (_1631)
        {
         _1630 = 1152;
        } // if _1631
        else
        {
         int32_t _1632 = 0 - 1152;
         _1630 = _1632;
        } // if _1631 else
        int32_t _1633 = _1630;
        uint32_t _1634 = (uint32_t)(_1633);
        uint32_t _1635 = _1634;
        int32_t _1636 = (int32_t)(_1635);
        int32_t _1637 = _1629 & _1636;
        int32_t _1638 = _1628 + _1637;
        int32_t _1639 = _1638 / 144;
        int32_t _1640 = _1639 * 144;
        int32_t _1641 = _1638 - _1640;
        int32_t _1642 = _1641 >> 31;
        int32_t _1643 = 144 >> 31;
        int32_t _1644 = _1642 & _1643;
        int32_t _1645 = _1639 - _1644;
        int32_t _1646 = ~_1643;
        int32_t _1647 = _1642 & _1646;
        int32_t _1648 = _1645 + _1647;
        int32_t _1649 = _1648 * 12;
        int32_t _1650 = _1627 - _1649;
        int32_t _1651 = _cSerializer_s0_i_j_ii_jj_iii_jjj % 82944;
        int32_t _1652 = _1651 >> 31;
        int32_t _1653;
        bool _1654 = 82944 > 0;
        if (_1654)
        {
         _1653 = 82944;
        } // if _1654
        else
        {
         int32_t _1655 = 0 - 82944;
         _1653 = _1655;
        } // if _1654 else
        int32_t _1656 = _1653;
        uint32_t _1657 = (uint32_t)(_1656);
        uint32_t _1658 = _1657;
        int32_t _1659 = (int32_t)(_1658);
        int32_t _1660 = _1652 & _1659;
        int32_t _1661 = _1651 + _1660;
        int32_t _1662 = _1661 / 9216;
        int32_t _1663 = _1662 * 9216;
        int32_t _1664 = _1661 - _1663;
        int32_t _1665 = _1664 >> 31;
        int32_t _1666 = 9216 >> 31;
        int32_t _1667 = _1665 & _1666;
        int32_t _1668 = _1662 - _1667;
        int32_t _1669 = ~_1666;
        int32_t _1670 = _1665 & _1669;
        int32_t _1671 = _1668 + _1670;
        int32_t _1672 = _1671 * 96;
        int32_t _1673 = _1650 - _1672;
        int32_t _1674 = _1615 + _1673;
        int32_t _1675 = _413 * _415;
        int32_t _1676 = _1675 + _410;
        int32_t _1677 = _1674 - _1676;
        int32_t _1678 = _1677 - _415;
        int32_t _1679 = _1678 + -1;
        float _1680 = ((const float *)_C)[_1679];
        _1405 = _1680;
       } // if _1437 else
       float _1681 = _1405;
       int32_t _1682 = _addr_temp;
       _cSerializer[_1682] = _1681;
       int32_t _1683 = _addr_temp;
       int32_t _1684 = _1683 + 1;
       _addr_temp = _1684;
      } // for _cSerializer_s0_i_j_ii_jj_iii_jjj
     } // alloc _addr_temp
     bool _1685 = (bool)(ADD_UINT64_T_SUFFIX(1));
     int32_t _1686 = _halide_buffer_set_host_dirty(_cSerializer_buffer, _1685);
     (void)_1686;
     // consume cSerializer
     // produce cLoader
     struct halide_device_interface_t const *_1687 = halide_opencl_device_interface();
     int32_t _1688 = halide_copy_to_device(_ucon, _cSerializer_buffer, _1687);
     bool _1689 = _1688 == 0;
     if (!_1689)      {
      return _1688;
     }
     status = clSetKernelArg(kernel[current_kernel], 0, sizeof(cl_mem), (void *)&((device_handle *)_halide_buffer_get_device(_cSerializer_buffer))->mem);
     CHECK(status);
     current_kernel++;

     // consume cLoader
     // produce Out
     status = clSetKernelArg(kernel[current_kernel], 0, sizeof(float), (void *)&_Alpha);
     CHECK(status);
     status = clSetKernelArg(kernel[current_kernel], 1, sizeof(float), (void *)&_Beta);
     CHECK(status);
     current_kernel++;

     // consume Out
     // produce drainer
     // consume drainer
     // produce collector
     // consume collector
     struct halide_dimension_t s9[6] = {
      {0, 12, 1, 0},
      {0, 12, 12, 0},
      {0, 8, 144, 0},
      {0, 8, 1152, 0},
      {0, 9, 9216, 0},
      {0, 4, 82944, 0},
     };
     struct halide_dimension_t *_1690 = s9;
     struct halide_dimension_t * _t197 = _1690;
     halide_buffer_t b5;
     struct halide_buffer_t *_1691 = &b5;
     uint64_t _1692 = (uint64_t)(ADD_UINT64_T_SUFFIX(0));
     void *_1693 = (void *)(_1692);
     struct halide_device_interface_t *_1694 = (struct halide_device_interface_t *)(_1692);
     struct halide_buffer_t *_1695 = _halide_buffer_init(_1691, _t197, _1693, _1692, _1694, 2, 32, 6, _t197, _1692);
     struct halide_buffer_t * _unloader_buffer = _1695;
     struct halide_device_interface_t const *_1696 = halide_opencl_device_interface();
     int32_t _1697 = halide_device_and_host_malloc(_ucon, _unloader_buffer, _1696);
     bool _1698 = _1697 == 0;
     if (!_1698)      {
      return _1697;
     }
     struct s10 { void * const ucon; void * const arg; s10(void *ucon, void *a) : ucon(ucon), arg((void *)a) {} ~s10() { halide_device_and_host_free_as_destructor(ucon, arg); } } d3(_ucon, _unloader_buffer);
     void *_1699 = 0;
     (void)_1699;
     {
      void *_1700 = _halide_buffer_get_host(_unloader_buffer);
      float *_unloader = (float *)(_1700);
      if (!_unloader)
      {
       return halide_error_out_of_memory(_ucon);
      }
      HalideFreeHelper _unloader_free(_ucon, _unloader, halide_device_host_nop_free);
      // produce unloader
      struct halide_device_interface_t const *_1701 = halide_opencl_device_interface();
      int32_t _1702 = halide_device_malloc(_ucon, _unloader_buffer, _1701);
      bool _1703 = _1702 == 0;
      if (!_1703)       {
       return _1702;
      }
      status = clSetKernelArg(kernel[current_kernel], 0, sizeof(cl_mem), (void *)&((device_handle *)_halide_buffer_get_device(_unloader_buffer))->mem);
      CHECK(status);
      current_kernel++;

      int32_t _1704 = halide_opencl_wait_for_kernels_finish(_ucon);
      bool _1705 = _1704 == 0;
      if (!_1705)       {
       return _1704;
      }
      bool _1706 = (bool)(ADD_UINT64_T_SUFFIX(1));
      int32_t _1707 = _halide_buffer_set_device_dirty(_unloader_buffer, _1706);
      (void)_1707;
      // consume unloader
      // produce deserializer
      int32_t _1708 = halide_copy_to_host(_ucon, _unloader_buffer);
      bool _1709 = _1708 == 0;
      if (!_1709)       {
       return _1708;
      }
      int32_t _1710 = halide_copy_to_host(_ucon, _deserializer_buffer);
      bool _1711 = _1710 == 0;
      if (!_1711)       {
       return _1710;
      }
      {
       int32_t _addr_temp;
       _addr_temp = 0;
       for (int _deserializer_s0_i_j_ii_jj_iii_jjj = 0; _deserializer_s0_i_j_ii_jj_iii_jjj < 0 + 331776; _deserializer_s0_i_j_ii_jj_iii_jjj++)
       {
        int32_t _1712 = _addr_temp;
        float _1713 = _unloader[_1712];
        int32_t _1714 = _deserializer_s0_i_j_ii_jj_iii_jjj / 82944;
        int32_t _1715 = _1714 * 82944;
        int32_t _1716 = _deserializer_s0_i_j_ii_jj_iii_jjj - _1715;
        int32_t _1717 = _1716 >> 31;
        int32_t _1718 = 82944 >> 31;
        int32_t _1719 = _1717 & _1718;
        int32_t _1720 = _1714 - _1719;
        int32_t _1721 = ~_1718;
        int32_t _1722 = _1717 & _1721;
        int32_t _1723 = _1720 + _1722;
        int32_t _1724 = _1723 * _436;
        int32_t _1725 = _deserializer_s0_i_j_ii_jj_iii_jjj % 82944;
        int32_t _1726 = _1725 >> 31;
        int32_t _1727;
        bool _1728 = 82944 > 0;
        if (_1728)
        {
         _1727 = 82944;
        } // if _1728
        else
        {
         int32_t _1729 = 0 - 82944;
         _1727 = _1729;
        } // if _1728 else
        int32_t _1730 = _1727;
        uint32_t _1731 = (uint32_t)(_1730);
        uint32_t _1732 = _1731;
        int32_t _1733 = (int32_t)(_1732);
        int32_t _1734 = _1726 & _1733;
        int32_t _1735 = _1725 + _1734;
        int32_t _1736 = _1735 / 9216;
        int32_t _1737 = _1736 * 9216;
        int32_t _1738 = _1735 - _1737;
        int32_t _1739 = _1738 >> 31;
        int32_t _1740 = 9216 >> 31;
        int32_t _1741 = _1739 & _1740;
        int32_t _1742 = _1736 - _1741;
        int32_t _1743 = ~_1740;
        int32_t _1744 = _1739 & _1743;
        int32_t _1745 = _1742 + _1744;
        int32_t _1746 = _1745 * _433;
        int32_t _1747 = _deserializer_s0_i_j_ii_jj_iii_jjj % 9216;
        int32_t _1748 = _1747 >> 31;
        int32_t _1749;
        bool _1750 = 9216 > 0;
        if (_1750)
        {
         _1749 = 9216;
        } // if _1750
        else
        {
         int32_t _1751 = 0 - 9216;
         _1749 = _1751;
        } // if _1750 else
        int32_t _1752 = _1749;
        uint32_t _1753 = (uint32_t)(_1752);
        uint32_t _1754 = _1753;
        int32_t _1755 = (int32_t)(_1754);
        int32_t _1756 = _1748 & _1755;
        int32_t _1757 = _1747 + _1756;
        int32_t _1758 = _1757 / 1152;
        int32_t _1759 = _1758 * 1152;
        int32_t _1760 = _1757 - _1759;
        int32_t _1761 = _1760 >> 31;
        int32_t _1762 = 1152 >> 31;
        int32_t _1763 = _1761 & _1762;
        int32_t _1764 = _1758 - _1763;
        int32_t _1765 = ~_1762;
        int32_t _1766 = _1761 & _1765;
        int32_t _1767 = _1764 + _1766;
        int32_t _1768 = _1767 * _430;
        int32_t _1769 = _deserializer_s0_i_j_ii_jj_iii_jjj % 1152;
        int32_t _1770 = _1769 >> 31;
        int32_t _1771;
        bool _1772 = 1152 > 0;
        if (_1772)
        {
         _1771 = 1152;
        } // if _1772
        else
        {
         int32_t _1773 = 0 - 1152;
         _1771 = _1773;
        } // if _1772 else
        int32_t _1774 = _1771;
        uint32_t _1775 = (uint32_t)(_1774);
        uint32_t _1776 = _1775;
        int32_t _1777 = (int32_t)(_1776);
        int32_t _1778 = _1770 & _1777;
        int32_t _1779 = _1769 + _1778;
        int32_t _1780 = _1779 / 144;
        int32_t _1781 = _1780 * 144;
        int32_t _1782 = _1779 - _1781;
        int32_t _1783 = _1782 >> 31;
        int32_t _1784 = 144 >> 31;
        int32_t _1785 = _1783 & _1784;
        int32_t _1786 = _1780 - _1785;
        int32_t _1787 = ~_1784;
        int32_t _1788 = _1783 & _1787;
        int32_t _1789 = _1786 + _1788;
        int32_t _1790 = _1789 * _427;
        int32_t _1791 = _deserializer_s0_i_j_ii_jj_iii_jjj % 144;
        int32_t _1792 = _1791 >> 31;
        int32_t _1793;
        bool _1794 = 144 > 0;
        if (_1794)
        {
         _1793 = 144;
        } // if _1794
        else
        {
         int32_t _1795 = 0 - 144;
         _1793 = _1795;
        } // if _1794 else
        int32_t _1796 = _1793;
        uint32_t _1797 = (uint32_t)(_1796);
        uint32_t _1798 = _1797;
        int32_t _1799 = (int32_t)(_1798);
        int32_t _1800 = _1792 & _1799;
        int32_t _1801 = _1791 + _1800;
        int32_t _1802 = _1801 / 12;
        int32_t _1803 = _1802 * 12;
        int32_t _1804 = _1801 - _1803;
        int32_t _1805 = _1804 >> 31;
        int32_t _1806 = 12 >> 31;
        int32_t _1807 = _1805 & _1806;
        int32_t _1808 = _1802 - _1807;
        int32_t _1809 = ~_1806;
        int32_t _1810 = _1805 & _1809;
        int32_t _1811 = _1808 + _1810;
        int32_t _1812 = _1811 * _424;
        int32_t _1813 = _deserializer_s0_i_j_ii_jj_iii_jjj % 12;
        int32_t _1814 = _1813 >> 31;
        int32_t _1815;
        bool _1816 = 12 > 0;
        if (_1816)
        {
         _1815 = 12;
        } // if _1816
        else
        {
         int32_t _1817 = 0 - 12;
         _1815 = _1817;
        } // if _1816 else
        int32_t _1818 = _1815;
        uint32_t _1819 = (uint32_t)(_1818);
        uint32_t _1820 = _1819;
        int32_t _1821 = (int32_t)(_1820);
        int32_t _1822 = _1814 & _1821;
        int32_t _1823 = _1813 + _1822;
        int32_t _1824 = _1812 + _1823;
        int32_t _1825 = _1790 + _1824;
        int32_t _1826 = _1768 + _1825;
        int32_t _1827 = _1746 + _1826;
        int32_t _1828 = _1724 + _1827;
        int32_t _1829 = _434 * _436;
        int32_t _1830 = _431 * _433;
        int32_t _1831 = _428 * _430;
        int32_t _1832 = _425 * _427;
        int32_t _1833 = _422 * _424;
        int32_t _1834 = _1833 + _419;
        int32_t _1835 = _1832 + _1834;
        int32_t _1836 = _1831 + _1835;
        int32_t _1837 = _1830 + _1836;
        int32_t _1838 = _1829 + _1837;
        int32_t _1839 = _1828 - _1838;
        ((float *)_deserializer)[_1839] = _1713;
        int32_t _1840 = _addr_temp;
        int32_t _1841 = _1840 + 1;
        _addr_temp = _1841;
       } // for _deserializer_s0_i_j_ii_jj_iii_jjj
      } // alloc _addr_temp
      bool _1842 = (bool)(ADD_UINT64_T_SUFFIX(1));
      int32_t _1843 = _halide_buffer_set_host_dirty(_deserializer_buffer, _1842);
      (void)_1843;
      int32_t _1844 = halide_device_and_host_free(_ucon, _unloader_buffer);
      bool _1845 = _1844 == 0;
      if (!_1845)       {
       return _1844;
      }
      _unloader_free.free();
     } // alloc _unloader
     _cSerializer_free.free();
    } // alloc _cSerializer
    _bSerializer_free.free();
   } // alloc _bSerializer
   _aSerializer_free.free();
  } // alloc _aSerializer
 } // if _463
 return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif

