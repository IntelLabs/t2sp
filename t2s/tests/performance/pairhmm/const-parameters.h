#ifndef PAIRHMM_CONST_PARAMS_H
#define PAIRHMM_CONST_PARAMS_H

// Inner loop bounds, which are static constant parameters of the design
#ifdef GPU
    #define OR          16
    #define OH          16
    #define OI          1
    #define OJ          1
    #define RR          8
    #define HH          8
    #define II          8
    #define JJ          8
#else // FPGA
    #ifdef TINY
        #define OR          4
        #define OH          4
        #define OI          4
        #define OJ          4
        #define RR          4
        #define HH          4
        #define II          4
        #define JJ          4
    #elif S10
        #define OR          32
        #define OH          32
        #define OI          16
        #define OJ          16
        #define RR          14
        #define HH          14
        #define II          16
        #define JJ          24
    #else
        #define OR          32
        #define OH          32
        #define OI          16
        #define OJ          16
        #define RR          14
        #define HH          14
        #define II          8
        #define JJ          24
    #endif
#endif

#define NUM_READS       (OR * RR)
#define NUM_HAPS        (OH * HH)
#define READ_LEN        (OI * II)
#define HAP_LEN         (OJ * JJ)

#endif
