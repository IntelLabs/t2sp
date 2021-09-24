#ifndef SIZES_H
#define SIZES_H

#ifdef TINY
    #define II          4
    #define JJ          4
    #define KK          4
    #define III         4
    #define JJJ         4
    #define KKK         4
    #define O_I         4
    #define O_J         4
    #define O_K         4
#elif S10
    #define II          32
    #define JJ          32
    #define KK          32
    #define III         12
    #define JJJ         16
    #define KKK         16
    #define O_I         32
    #define O_J         32
    #define O_K         4
#else
    #define II          32
    #define JJ          32
    #define KK          32
    #define III         10
    #define JJJ         8
    #define KKK         16
    #define O_I         32
    #define O_J         32
    #define O_K         4
#endif

#endif