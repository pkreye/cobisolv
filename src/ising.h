#ifdef __cplusplus
extern "C" {
#endif

typedef struct {

} ising_data;

extern bool ising_established(void);

extern int ising_init(void);

extern void ising_close(void);

extern void ising_solver(double **val, int maxNodes, int8_t *Q);

#ifdef __cplusplus
}
#endif
