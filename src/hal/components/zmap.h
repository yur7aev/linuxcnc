#ifndef ZMAP_H
#define ZMAP_H

#define SHMEM_KEY 4712 // a famous and very old-fashioned german perfume brand

typedef struct {
    int nx, ny;
    double x0, y0;
    double dx, dy;
    double z[1];
} zmap_shm;

#endif /* PROBEKINS_H */
