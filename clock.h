/* 사이클 카운터 사용을 위한 함수들 */

/* 카운터 시작 */
void start_counter();

/* 카운터 시작 이후 경과한 사이클 수 반환 */
double get_counter();

/* 카운터 자체의 오버헤드 측정 */
double ovhd();

/* 기본 sleep 시간으로 프로세서 클럭 속도 측정 */
double mhz(int verbose);

/* 정확도를 더 세밀하게 제어하며 프로세서 클럭 속도 측정 */
double mhz_full(int verbose, int sleeptime);

/** 타이머 인터럽트 오버헤드를 보정하는 특수 카운터 */

void start_comp_counter();

double get_comp_counter();
