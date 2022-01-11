// Copyright(c) 2016-2018, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//******************************************************************************
// Somewhat complex mathematical operations
//******************************************************************************

#ifndef CONTEXT_H
#define CONTEXT_H

#include <cmath> // std::isinf
#include <algorithm> // std::min

#define MAX_QUAL                       254
#define MAX_JACOBIAN_TOLERANCE         8.0
#define JACOBIAN_LOG_TABLE_STEP        0.0001
#define JACOBIAN_LOG_TABLE_INV_STEP    (1.0 / JACOBIAN_LOG_TABLE_STEP)
#define JACOBIAN_LOG_TABLE_SIZE        ((uint) (MAX_JACOBIAN_TOLERANCE / JACOBIAN_LOG_TABLE_STEP) + 1)

using namespace std;

template< class NUMBER >
class ContextBase
{
    public:
        static NUMBER ph2pr[128];
        static NUMBER INITIAL_CONSTANT;
        static NUMBER LOG10_INITIAL_CONSTANT;
        static NUMBER RESULT_THRESHOLD;

        static bool staticMembersInitializedFlag;
        static NUMBER jacobianLogTable[JACOBIAN_LOG_TABLE_SIZE];
        static NUMBER matchToMatchProb[((MAX_QUAL + 1) * (MAX_QUAL + 2)) >> 1];

        static void initializeStaticMembers( void )
        {
            // Order of calls important - Jacobian first, then MatchToMatch
            initializeJacobianLogTable();
            initializeMatchToMatchProb();
        }

        static void deleteStaticMembers( void )
        {
            if( staticMembersInitializedFlag ) {
                staticMembersInitializedFlag = false;
            }
        }

        // Called only once during library load - don't bother to optimize with single precision fp
        static void initializeJacobianLogTable( void )
        {
            for( uint k = 0; k < JACOBIAN_LOG_TABLE_SIZE; k++ ) {
                jacobianLogTable[k] = (NUMBER) log10( 1.0 + pow( 10.0, -(k * 1.0 * JACOBIAN_LOG_TABLE_STEP) ) );
            }
        }

        // Called only once per library load - don't bother optimizing with single fp
        static void initializeMatchToMatchProb( void )
        {
            double LN10 = log( 10 );
            double INV_LN10 = 1.0 / LN10;

            for( uint i = 0, offset = 0; i <= MAX_QUAL; offset += ++i ) {
                for( uint j = 0; j <= i; j++ ) {
                    double log10Sum = approximateLog10SumLog10( (-0.1 * i), (-0.1 * j) );
                    double matchToMatchLog10 = log1p( -min( 1.0, pow( 10, log10Sum ) ) ) * INV_LN10;

                    matchToMatchProb[offset + j] = (NUMBER) pow( 10, matchToMatchLog10 );
                }
            }
        }

        // Called during computation - use single precision where possible
        static int fastRound( NUMBER d )
        {
            return (d > ((NUMBER) 0.0)) ? (int) (d + ((NUMBER ) 0.5)) : (int) (d - ((NUMBER) 0.5));
        }

        // Called during computation - use single precision where possible
        static NUMBER approximateLog10SumLog10( NUMBER small, NUMBER big )
        {
            // Make sure small is really the smaller value
            if( small > big ) {
                NUMBER t = big;
                big = small;
                small = t;
            }

            if( std::isinf( small ) == true || std::isinf( big ) == true ) {
                return big;
            }

            NUMBER diff = big - small;
            if( diff >= ((NUMBER) MAX_JACOBIAN_TOLERANCE) ) {
                return big;
            }

            // OK, so |y-x| < tol: we use the following identity then:
            // we need to compute log10(10^x + 10^y)
            // By Jacobian logarithm identity, this is equal to
            // max(x,y) + log10(1+10^-abs(x-y))
            // we compute the second term as a table lookup with integer quantization
            // we have pre-stored correction for 0,0.1,0.2,... 10.0
            uint ind = fastRound( (NUMBER) (diff * ((NUMBER) JACOBIAN_LOG_TABLE_INV_STEP)) ); // hard rounding
            return (big + jacobianLogTable[ind]);
        }
};

template< class NUMBER >
class Context : public ContextBase<NUMBER>
{};

template<>
class Context<double> : public ContextBase<double>
{
    public:
        Context( void ) : ContextBase<double>()
        {
            if( !staticMembersInitializedFlag ) {
                initializeStaticMembers();

                for( uint x = 0; x < 128; x++ ) {
                    ph2pr[x] = pow( 10.0, -( (double) x) / 10.0 );
                }

                INITIAL_CONSTANT = ldexp( 1.0, 1020.0 );
                LOG10_INITIAL_CONSTANT = log10( INITIAL_CONSTANT );
                RESULT_THRESHOLD = 0.0;

                staticMembersInitializedFlag = true;
            }
        }

        double LOG10( double v ) { return log10( v ); }
        inline double POW( double b, double e ) { return pow( b, e ); }

        static double _(double n){ return n; }
        static double _(float n){ return ((double) n); }

        inline double set_mm_prob( int insQual, int delQual ) {
            int minQual = delQual;
            int maxQual = insQual;

            if( insQual <= delQual ) {
                minQual = insQual;
                maxQual = delQual;
            }

            if( MAX_QUAL < maxQual ) {
                return 1.0 - POW( 10.0, approximateLog10SumLog10( -0.1 * minQual, -0.1 * maxQual ) );
            } else {
                return matchToMatchProb[((maxQual * (maxQual + 1)) >> 1) + minQual];
            }
        }
};

template<>
class Context<float> : public ContextBase<float>
{
    public:
        Context( void ) : ContextBase<float>()
        {
            if( !staticMembersInitializedFlag ) {
                initializeStaticMembers();

                for( uint x = 0; x < 128; x++ ) {
                    ph2pr[x] = powf( 10.f, -((float) x) / 10.f );
                }

                INITIAL_CONSTANT = ldexpf( 1.f, 120.f );
                LOG10_INITIAL_CONSTANT = log10f( INITIAL_CONSTANT );
                RESULT_THRESHOLD = ldexpf( 1.f, -110.f );

                staticMembersInitializedFlag = true;
            }
        }

        float LOG10( float v ) { return log10f( v ); }
        inline float POW( float b, float e ) { return powf( b, e ); }

        static float _( double n ) { return ((float) n); }
        static float _( float n ) { return n; }

        inline float set_mm_prob( int insQual, int delQual )
        {
            int minQual = delQual;
            int maxQual = insQual;

            if( insQual <= delQual ) {
                minQual = insQual;
                maxQual = delQual;
            }

            if( MAX_QUAL < maxQual ) {
                return 1.0f - POW( 10.0f, approximateLog10SumLog10( -0.1f * minQual, -0.1f * maxQual ) );
            } else {
                return matchToMatchProb[((maxQual * (maxQual + 1)) >> 1) + minQual];
            }
        }
};

template< typename NUMBER >
NUMBER ContextBase<NUMBER>::ph2pr[128];
template< typename NUMBER >
NUMBER ContextBase<NUMBER>::INITIAL_CONSTANT;
template< typename NUMBER >
NUMBER ContextBase<NUMBER>::LOG10_INITIAL_CONSTANT;
template< typename NUMBER >
NUMBER ContextBase<NUMBER>::RESULT_THRESHOLD;
template< typename NUMBER >
bool ContextBase<NUMBER>::staticMembersInitializedFlag = false;
template< typename NUMBER >
NUMBER ContextBase<NUMBER>::jacobianLogTable[JACOBIAN_LOG_TABLE_SIZE];
template< typename NUMBER >
NUMBER ContextBase<NUMBER>::matchToMatchProb[((MAX_QUAL + 1) * (MAX_QUAL + 2)) >> 1];

#endif /* CONTEXT_H */
