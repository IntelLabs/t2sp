#ifndef PAIRHMM_CONST_PARAMS_H
#define PAIRHMM_CONST_PARAMS_H

#ifdef TINY
    #define OR          4
    #define OH          4
    #define OI          4
    #define OJ          4
    #define RR          4
    #define HH          4
    #define II          4
    #define JJ          4
#else
    #define OR          32
    #define OH          32
    #define OI          16
    #define OJ          16
    #define RR          8
    #define HH          8
    #define II          8
    #define JJ          24
#endif

#define NUM_READS       (OR * RR)
#define NUM_HAPS        (OH * HH)
#define READ_LEN        (OI * II)
#define HAP_LEN         (OJ * JJ)

#endif