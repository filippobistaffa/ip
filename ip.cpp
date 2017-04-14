#include "ip.h"

typedef struct {
	const std::vector<value> *vals;
	value maxval;
} maxdata;

__attribute__((always_inline)) inline
void initialise(int *a, unsigned n, unsigned m) {

	a[0] = n - m + 1;

	for (unsigned i = 1; i < m; ++i)
		a[i] = 1;

	a[m] = -1;
}

__attribute__((always_inline)) inline
void printpart(int *a, unsigned n, void *data) {

	printbuf(a, n);
}

__attribute__((always_inline)) inline
void computehist(const int *a, unsigned n, unsigned *hist) {

	for (unsigned i = 0; i < n; ++i)
		hist[a[i]]++;
}

template <typename type>
__attribute__((always_inline)) inline
type reducebuf(const type *buf, unsigned n) {

	type ret = 0;

	for (unsigned i = 0; i < n; ++i)
		ret += buf[i];

	return ret;
}

//#include <assert.h>

__attribute__((always_inline)) inline
void maxvaluepart(int *a, unsigned n, void *data) {

	maxdata *md = (maxdata *)data;
	unsigned hist[K + 1] = { 0 };
	computehist(a, n, hist);
	value val = 0;

	// If the number of cars exceeds the number of drivers, the integer partition is unfeasible
	if (reducebuf(hist + 2, K - 1) > _D)
		return;

	for (unsigned k = 1; k <= K; ++k) {
		//printf("k = %u, hist[%u] = %u, md->difpfx[%u].size() = %lu\n", k, k, hist[k], k, md->difpfx[k].size());
		if (hist[k] > md->vals[k].size()) return;
		if (hist[k] == 0) continue;
		val += md->vals[k][hist[k] - 1];
	}

	//printbuf(hist, K + 1, "hist");
	//printbuf(a, n, NULL, NULL, " = ");
	//printf("%f\n", val);
	if (val > md->maxval) md->maxval = val;
}

__attribute__((always_inline)) inline
void conjugate(const int *a, unsigned m, void (*cf)(int *, unsigned, void *), void *data) {

	int c[a[0]];
	unsigned i = 0;

	while (1) {
		c[i++] = m;
		while (i >= a[m - 1]) {
			m--;
			if (m == 0) goto exit;
		}
	}

exit:	cf(c, i, data);
}

__attribute__((always_inline)) inline
size_t enumerate(int *a, unsigned m, void (*pf)(int *, unsigned, void *), void (*cf)(int *, unsigned, void *), void *data) {

	// The algorithm follows Knuth v4 fasc3 p38 in rough outline;
	// Knuth credits it to Hindenburg, 1779.

	size_t count = 0;

	while (1) {

		count++;
		if (pf) pf(a, m, data);
		if (cf) conjugate(a, m, cf, data);
		if (m == 1) return count;
		if (a[0] - 1 > a[1]) {
			a[0]--;
			a[1]++;
			continue;
		}

		int j = 2;
		int s = a[0] + a[1] - 1;

		while (j < m && a[j] >= a[0] - 1) {
		    s += a[j];
		    j++;
		}

		if (j >= m) return count;
		int x = a[j] + 1;
		a[j] = x;
		j--;

		while (j > 0) {
		    a[j] = x;
		    s -= x;
		    j--;
		}

		a[0] = s;
	}
}

value maxpartition(const std::vector<value> *vals) {

	int a[K + 1];
	size_t count = 0;
	maxdata md = { .vals = vals, .maxval = 0 };

	for (unsigned m = 1; m <= K; ++m) {
		initialise(a, _N, m);
		size_t c = enumerate(a, m, NULL, maxvaluepart, &md);
		//printf("%zu partition(s) with max value = %u\n", c, m);
		count += c;
	}

	printf("%zu total integer partition(s)\n", count);
	return md.maxval;
}

__attribute__((always_inline)) inline
void createedge(edge *g, agent v1, agent v2, edge e) {

	#ifdef DOT
	printf("\t%u -- %u [label = \"e_%u,%u\"];\n", v1, v2, v1, v2);
	#endif
	g[v1 * _N + v2] = g[v2 * _N + v1] = e;
}

#ifdef TWITTER

__attribute__((always_inline)) inline
edge twitter(const char *filename, edge *g) {

	#define MAXLINE 1000
	static char line[MAXLINE];
	FILE *f = fopen(filename, "rt");
	fgets(line, MAXLINE, f);
	edge ne = atoi(line);

	for (edge i = 0; i < ne; i++) {
		fgets(line, MAXLINE, f);
		const agent v1 = atoi(line);
		fgets(line, MAXLINE, f);
		const agent v2 = atoi(line);
		createedge(g, v1, v2, _N + i);
	}

	fclose(f);

	return ne;
}

#else

__attribute__((always_inline)) inline
edge scalefree(edge *g) {

	edge ne = 0;
	agent deg[_N] = {0};

	for (agent i = 1; i <= _M; i++) {
		for (agent j = 0; j < i; j++) {
			createedge(g, i, j, _N + ne);
			deg[i]++;
			deg[j]++;
			ne++;
		}
	}

	chunk t[_C] = { 0 };
	chunk t1[_C] = { 0 };

	for (agent i = _M + 1; i < _N; i++) {
		ONES(t1, i, _C);
		MASKANDNOT(t, t1, t, _C);
		for (agent j = 0; j < _M; j++) {
			agent d = 0;
			for (agent h = 0; h < i; h++)
				if (!GET(t, h)) d += deg[h];
			if (d > 0) {
				int p = nextInt(d);
				agent q = 0;
				while (p >= 0) {
					if (!GET(t, q)) p = p - deg[q];
					q++;
				}
				q--;
				SET(t, q);
				createedge(g, i, q, _N + ne);
				deg[i]++;
				deg[q]++;
				ne++;
			}
		}
	}

	return ne;
}

#endif

#define MAXRAND 10

value randomvalue(agent *c, agent nl, void *data) {

	return nextFloat() * MAXRAND;
}

typedef struct {
	value (*cf)(agent *, agent, void *);
	const std::vector<value> *vals;
	void *data;
} coaldata;

template <typename type>
void maxsr(agent *c, agent nl, const edge *g, const agent *adj, const chunk *l, type *data) {

	printbuf(c + 1, *c, NULL, NULL, " = ");
	printf("%f\n", data->cf(c, nl, data->data));
}

int main(int argc, char *argv[]) {

	unsigned seed = atoi(argv[1]);
	meter *sp = createsp(seed);

	// Create leaders array

	agent la[_N] = {0};
	chunk l[_C] = {0};

	for (agent i = 0; i < _D; i++)
		la[i] = 1;

	shuffle(la, _N, sizeof(agent));

	for (agent i = 0; i < _N; i++)
		if (la[i]) SET(l, i);

	// Create/read graph

	init(seed);
	edge *g = (edge *)calloc(_N * _N, sizeof(edge));

	#ifdef DOT
	printf("graph G {\n");
	#endif
	#ifdef TWITTER
	twitter(argv[2], g);
	#else
	scalefree(g);
	#endif
	#ifdef DOT
	printf("}\n\n");
	#endif

	puts("Adjacency matrix");
	for (agent i = 0; i < _N; i++)
		printbuf(g + i * _N, _N, NULL, "% 3u");
	puts("");
	printbuf(la, _N, "l");
	puts("");

	vector<value> vals[K + 1];
	coaldata cd;
	cd.vals = vals;
	cd.cf = srvalue;
	cd.data = sp;

	coalitions(g, maxsr, &cd, K, l, MAXDRIVERS);

	free(sp);
	free(g);

	return 0;
}
