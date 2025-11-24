// DeadStore.cpp input with multiple pointers
void demo(int *A) {
  int *p_a = A;
  int *p_b = p_a;
  *p_a = 1;
  *p_b = 2;
}