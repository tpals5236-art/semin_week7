#ifndef __CONFIG_H_
#define __CONFIG_H_

/*
 * config.h - malloc lab 설정 파일
 *
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */

/*
 * 드라이버가 기본 trace 파일을 찾을 때 사용하는 기본 경로이다.
 * 실행 시 -t 옵션으로 이 값을 덮어쓸 수 있다.
 */
#define TRACEDIR "./traces/"

/*
 * 드라이버가 테스트에 사용할 TRACEDIR 내 기본 trace 파일 목록이다.
 * 테스트 세트에 trace를 추가하거나 삭제하려면 이 목록을 수정하면 된다.
 * 예를 들어 학생들에게 realloc 구현을 요구하지 않는다면 마지막 두
 * trace를 삭제할 수 있다.
 */
#define DEFAULT_TRACEFILES \
  "amptjp-bal.rep",\
  "cccp-bal.rep",\
  "cp-decl-bal.rep",\
  "expr-bal.rep",\
  "coalescing-bal.rep",\
  "random-bal.rep",\
  "random2-bal.rep",\
  "binary-bal.rep",\
  "binary2-bal.rep",\
  "realloc-bal.rep",\
  "realloc2-bal.rep"

/*
 * 이 상수는 기준 시스템에서 우리 trace를 사용해 측정한 libc malloc의
 * 예상 성능을 나타낸다. 보통 학생들이 사용하는 것과 비슷한 시스템을
 * 기준으로 잡는다. 목적은 throughput이 성능 지수에 기여하는 상한을
 * 정하는 것이다. 학생이 AVG_LIBC_THRUPUT을 넘어서면 점수에서 더
 * 이상 이득을 얻지 못한다. 이는 지나치게 빠르지만 매우 비정상적인
 * malloc 패키지를 만드는 일을 억제하기 위한 장치다.
 */
#define AVG_LIBC_THRUPUT      600E3  /* 600 Kops/sec */

 /* 
  * 이 상수는 공간 활용도(UTIL_WEIGHT)와 처리량
  * (1 - UTIL_WEIGHT)이 성능 지수에 얼마나 반영되는지를 정한다.
  */
#define UTIL_WEIGHT .60

/* 
 * 정렬 요구사항(바이트 단위, 4 또는 8)
 */
#define ALIGNMENT 8  

/* 
 * 최대 힙 크기(바이트 단위)
 */
#define MAX_HEAP (20*(1<<20))  /* 20 MB */

/*****************************************************************************
 * 타이밍 방법을 선택하려면 아래 USE_xxx 상수 중 정확히 하나만 1로 설정한다
 *****************************************************************************/
#define USE_FCYC   0   /* K-best 방식의 사이클 카운터 사용 (x86, Alpha 전용) */
#define USE_ITIMER 0   /* interval timer 사용 (대부분의 Unix 환경) */
#define USE_GETTOD 1   /* gettimeofday 사용 (대부분의 Unix 환경) */

#endif /* __CONFIG_H */
