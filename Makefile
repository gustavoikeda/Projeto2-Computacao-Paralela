CC     = gcc
CFLAGS = -O2 -fopenmp

TARGETS = analyzer_seq \
          analyzer_par_atomic \
          analyzer_par_critical \
          analyzer_par_lock \
          analyzer_par_atomic_padded

all: $(TARGETS)

analyzer_seq:
	$(CC) $(CFLAGS) analyzer_seq.c hash_table.c -o analyzer_seq

analyzer_par_atomic:
	$(CC) $(CFLAGS) analyzer_par_atomic.c hash_table.c -o analyzer_par_atomic

analyzer_par_critical:
	$(CC) $(CFLAGS) analyzer_par_critical.c hash_table.c -o analyzer_par_critical

analyzer_par_lock:
	$(CC) $(CFLAGS) analyzer_par_lock.c hash_table.c -o analyzer_par_lock

analyzer_par_atomic_padded:
	$(CC) $(CFLAGS) analyzer_par_atomic_padded.c hash_table_padded.c -o analyzer_par_atomic_padded

clean:
	rm -f $(TARGETS)