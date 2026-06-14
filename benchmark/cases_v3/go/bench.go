// CASES_V3 Go Benchmark Suite — Standard Library Only
//
// Skipped benchmarks:
//   2 (nbody):          Heavy FP — Go's float64 is fine but contract suggests skip
//   3 (spectral_norm):  Heavy FP — same rationale
//   13 (alloc_small_churn):   Manual alloc/dealloc — Go is GC'd, results misleading
//   14 (alloc_large_objects): Manual alloc/dealloc — Go is GC'd, results misleading
//   17 (rc_vs_gc_trace):      No Rc/Arc in Go stdlib; GC-owned references
//   28 (c_ffi_call_hotloop):  Requires CGo (cgo disabled for pure-Go build)
//   29 (c_buffer_handoff):    Requires CGo (cgo disabled for pure-Go build)
//
// Compile: go build -ldflags="-s -w"

package main

import (
	"fmt"
	"io"
	"math/big"
	"net"
	"os"
	"os/exec"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
)

// ============================================================================
// SHARED CONSTANTS
// ============================================================================

const RANDOM_SEED = 42
const MODULUS = 1000000007

// ============================================================================
// SHARED HELPERS
// ============================================================================

// LCG — Deterministic Linear Congruential Generator
type LCG struct {
	state uint64
}

func NewLCG() *LCG {
	return &LCG{state: RANDOM_SEED}
}

func (r *LCG) Reset() {
	r.state = RANDOM_SEED
}

func (r *LCG) Next() uint64 {
	r.state = (r.state*1103515245 + 12345) & 0x7fffffff
	return r.state
}

// hashString — djb2 hash
func hashString(s string) uint64 {
	h := uint64(5381)
	for i := 0; i < len(s); i++ {
		h = (h << 5) + h + uint64(s[i])
	}
	return h
}

// randomString — generate random alphanumeric string of length [minLen, maxLen]
func randomString(r *LCG, minLen, maxLen int) string {
	l := minLen + int(r.Next()%uint64(maxLen-minLen+1))
	buf := make([]byte, l)
	const chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	for i := 0; i < l; i++ {
		buf[i] = chars[r.Next()%uint64(len(chars))]
	}
	return string(buf)
}

// randomInt64Array — fill slice with LCG values
func randomInt64Array(r *LCG, a []int64) {
	for i := range a {
		a[i] = int64(r.Next())
	}
}

// clampInt64 — clamp v to [lo, hi]
func clampInt64(v, lo, hi int64) int64 {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

// ============================================================================
// BSTMap — ordered map for btree_scan (no stdlib ordered map in Go)
// ============================================================================

type bstNode struct {
	key         int
	value       int
	left, right *bstNode
}

type BSTMap struct {
	root *bstNode
	size int
}

func (m *BSTMap) Insert(key, value int) {
	m.root = m.insert(m.root, key, value)
}

func (m *BSTMap) insert(n *bstNode, key, value int) *bstNode {
	if n == nil {
		m.size++
		return &bstNode{key: key, value: value}
	}
	if key < n.key {
		n.left = m.insert(n.left, key, value)
	} else if key > n.key {
		n.right = m.insert(n.right, key, value)
	} else {
		n.value = value // update — no size change
	}
	return n
}

func (m *BSTMap) Delete(key int) {
	m.root = m.delete(m.root, key)
}

func (m *BSTMap) delete(n *bstNode, key int) *bstNode {
	if n == nil {
		return nil
	}
	if key < n.key {
		n.left = m.delete(n.left, key)
	} else if key > n.key {
		n.right = m.delete(n.right, key)
	} else {
		m.size--
		if n.left == nil {
			return n.right
		}
		if n.right == nil {
			return n.left
		}
		// Two children: find inorder successor (min of right subtree)
		succ := m.findMin(n.right)
		n.key = succ.key
		n.value = succ.value
		n.right = m.delete(n.right, succ.key)
	}
	return n
}

func (m *BSTMap) findMin(n *bstNode) *bstNode {
	for n.left != nil {
		n = n.left
	}
	return n
}

// Inorder — forward iteration, yield returns false to stop
func (m *BSTMap) Inorder(yield func(key, value int) bool) {
	m.inorder(m.root, yield)
}

func (m *BSTMap) inorder(n *bstNode, yield func(key, value int) bool) bool {
	if n == nil {
		return true
	}
	if !m.inorder(n.left, yield) {
		return false
	}
	if !yield(n.key, n.value) {
		return false
	}
	return m.inorder(n.right, yield)
}

// ReverseInorder — backward iteration
func (m *BSTMap) ReverseInorder(yield func(key, value int) bool) {
	m.reverseInorder(m.root, yield)
}

func (m *BSTMap) reverseInorder(n *bstNode, yield func(key, value int) bool) bool {
	if n == nil {
		return true
	}
	if !m.reverseInorder(n.right, yield) {
		return false
	}
	if !yield(n.key, n.value) {
		return false
	}
	return m.reverseInorder(n.left, yield)
}

// ============================================================================
// TIER 1: COMPUTE & ALGORITHM
// ============================================================================

// --- 1. binary_trees -------------------------------------------------------

type btNode struct {
	value       int
	left, right *btNode
}

func newTree(depth int) *btNode {
	if depth <= 0 {
		return nil
	}
	return &btNode{
		value: 1,
		left:  newTree(depth - 1),
		right: newTree(depth - 1),
	}
}

func treeSum(n *btNode) int {
	if n == nil {
		return 0
	}
	return n.value + treeSum(n.left) + treeSum(n.right)
}

func bench_binary_trees() int {
	const MIN_DEPTH = 4
	const MAX_DEPTH = 18

	checksum := 0
	for depth := MIN_DEPTH; depth <= MAX_DEPTH; depth += 2 {
		iterations := 1 << (MAX_DEPTH - depth + MIN_DEPTH)
		for i := 0; i < iterations; i++ {
			tree := newTree(depth)
			checksum = (checksum + treeSum(tree)) % MODULUS
		}
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 4. mandelbrot ---------------------------------------------------------

func bench_mandelbrot() int {
	const WIDTH = 800
	const HEIGHT = 800
	const MAX_ITER = 200
	const XMIN = -2.0
	const XMAX = 1.0
	const YMIN = -1.5
	const YMAX = 1.5

	checksum := int64(0)

	for py := 0; py < HEIGHT; py++ {
		ci := YMIN + (YMAX-YMIN)*float64(py)/HEIGHT
		for px := 0; px < WIDTH; px++ {
			cr := XMIN + (XMAX-XMIN)*float64(px)/WIDTH
			zr := 0.0
			zi := 0.0
			iter := 0

			for iter < MAX_ITER {
				zr2 := zr * zr
				zi2 := zi * zi
				if zr2+zi2 > 4.0 {
					break
				}
				zi = 2.0*zr*zi + ci
				zr = zr2 - zi2 + cr
				iter++
			}
			checksum += int64(iter)
		}
	}

	result := int(checksum % MODULUS)
	fmt.Printf("%d\n", result)
	return 0
}

// --- 5. fasta --------------------------------------------------------------

func bench_fasta() int {
	const N = 250000
	alu := "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGC" +
		"GGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAA" +
		"AATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCA" +
		"GGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGC" +
		"CTGGGCGACAGAGCGAGACTCCGTCTCAAAAA"

	// Count nucleotide frequencies in ALU
	freq := make(map[byte]int)
	for i := 0; i < len(alu); i++ {
		freq[alu[i]]++
	}

	// Build cumulative distribution: A, C, G, T
	bases := []byte{'A', 'C', 'G', 'T'}
	cumSum := make([]int, 4)
	total := 0
	for i, b := range bases {
		total += freq[b]
		cumSum[i] = total
	}

	r := NewLCG()
	checksum := 0

	for i := 0; i < N; i++ {
		val := r.Next() % uint64(total)
		var nucleotide byte
		for j := 0; j < 4; j++ {
			if val < uint64(cumSum[j]) {
				nucleotide = bases[j]
				break
			}
		}
		checksum = (checksum*31 + int(nucleotide)) % MODULUS
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 6. regex_redux --------------------------------------------------------

func bench_regex_redux() int {
	const N = 5000
	r := NewLCG()
	bases := []byte{'A', 'C', 'G', 'T'}
	dna := make([]byte, N)
	for i := 0; i < N; i++ {
		dna[i] = bases[r.Next()%4]
	}
	dnaStr := string(dna)

	re1 := regexp.MustCompile(`agggtaaa|tttaccct`)
	re2 := regexp.MustCompile(`tHa[Nt]`)

	count1 := len(re1.FindAllStringIndex(dnaStr, -1))
	count2 := len(re2.FindAllStringIndex(dnaStr, -1))

	totalCount := count1 + count2
	checksum := (totalCount * N) % MODULUS

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 7. pidigits (Brent-Salamin AGM with big.Float) -----------------------

func bench_pidigits() int {
	const N = 5000
	// Precision in bits — enough for N decimal digits plus guard bits
	prec := uint(N * 4)

	one := new(big.Float).SetPrec(prec).SetInt64(1)
	two := new(big.Float).SetPrec(prec).SetInt64(2)
	four := new(big.Float).SetPrec(prec).SetInt64(4)

	// Brent-Salamin AGM initialization
	a := new(big.Float).SetPrec(prec).SetInt64(1) // a_0 = 1
	b := new(big.Float).SetPrec(prec)
	b.Sqrt(two)       // sqrt(2)
	b.Quo(one, b)     // b_0 = 1/sqrt(2)
	t := new(big.Float).SetPrec(prec).Quo(one, four) // t_0 = 1/4
	p := new(big.Float).SetPrec(prec).SetInt64(1)    // p_0 = 1

	aNext := new(big.Float).SetPrec(prec)
	bNext := new(big.Float).SetPrec(prec)
	tNext := new(big.Float).SetPrec(prec)
	pNext := new(big.Float).SetPrec(prec)
	tmp := new(big.Float).SetPrec(prec)

	// 12 iterations yields > 5000 decimal digits (quadratic convergence)
	for i := 0; i < 12; i++ {
		// a_{n+1} = (a_n + b_n) / 2
		aNext.Add(a, b)
		aNext.Quo(aNext, two)

		// b_{n+1} = sqrt(a_n * b_n)
		tmp.Mul(a, b)
		bNext.Sqrt(tmp)

		// t_{n+1} = t_n - p_n * (a_n - a_{n+1})^2
		tmp.Sub(a, aNext)
		tmp.Mul(tmp, tmp)
		tmp.Mul(tmp, p)
		tNext.Sub(t, tmp)

		// p_{n+1} = 2 * p_n
		pNext.Mul(p, two)

		a.Set(aNext)
		b.Set(bNext)
		t.Set(tNext)
		p.Set(pNext)
	}

	// pi = (a + b)^2 / (4 * t)
	pi := new(big.Float).SetPrec(prec)
	pi.Add(a, b)
	pi.Mul(pi, pi)
	den := new(big.Float).SetPrec(prec).Mul(four, t)
	pi.Quo(pi, den)

	// Format as decimal string with N digits after decimal point
	s := pi.Text('f', N)

	// Checksum all digit characters (skip "3" and ".")
	checksum := 0
	for _, c := range s {
		if c >= '0' && c <= '9' {
			d := int(c - '0')
			checksum = (checksum*31 + d) % MODULUS
		}
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// ============================================================================
// TIER 2: DATA STRUCTURES
// ============================================================================

// --- 8. hashmap_heavy ------------------------------------------------------

func bench_hashmap_heavy() int {
	const N_KEYS = 100000
	const N_LOOKUPS = 5000000

	r := NewLCG()
	m := make(map[string]int, N_KEYS)
	keys := make([]string, N_KEYS)

	// 1. Generate keys and insert
	for i := 0; i < N_KEYS; i++ {
		k := randomString(r, 8, 16)
		keys[i] = k
		m[k] = i
	}

	// 2. Lookup storm
	checksum := 0
	for i := 0; i < N_LOOKUPS; i++ {
		idx := r.Next() % N_KEYS
		val, ok := m[keys[idx]]
		if ok {
			checksum = (checksum*31 + val) % MODULUS
		}
	}

	// 3. Delete every 4th key
	for i := 3; i < N_KEYS; i += 4 {
		delete(m, keys[i])
	}

	// 4. Re-lookup remaining keys
	for i := 0; i < N_LOOKUPS/2; i++ {
		idx := r.Next() % N_KEYS
		val, ok := m[keys[idx]]
		if ok {
			checksum = (checksum*31 + val) % MODULUS
		}
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 9. btree_scan ---------------------------------------------------------

func bench_btree_scan() int {
	const N_KEYS = 500000

	r := NewLCG()
	bmap := new(BSTMap)

	// 1. Insert N_KEYS random integers
	keys := make([]int, N_KEYS)
	for i := 0; i < N_KEYS; i++ {
		k := int(r.Next())
		keys[i] = k
		bmap.Insert(k, i)
	}

	checksum := 0

	// 2. Forward range scan
	bmap.Inorder(func(key, value int) bool {
		checksum = (checksum + (key*value)%MODULUS) % MODULUS
		return true
	})

	// 3. Reverse range scan
	bmap.ReverseInorder(func(key, value int) bool {
		checksum = (checksum + (key*value)%MODULUS) % MODULUS
		return true
	})

	// 4. Delete every 3rd key
	for i := 2; i < N_KEYS; i += 3 {
		bmap.Delete(keys[i])
	}

	// 5. Re-iterate
	bmap.Inorder(func(key, value int) bool {
		checksum = (checksum + (key*value)%MODULUS) % MODULUS
		return true
	})

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 10. sort_gauntlet -----------------------------------------------------

func bench_sort_gauntlet() int {
	const N = 1000000

	r := NewLCG()

	// Pass 1: random array
	arr1 := make([]int64, N)
	randomInt64Array(r, arr1)
	sort.Slice(arr1, func(i, j int) bool { return arr1[i] < arr1[j] })
	checksum := int64(0)
	for i := 0; i < N; i++ {
		checksum = (checksum*31 + arr1[i]) % MODULUS
	}

	// Pass 2: nearly-sorted (copy arr1, perturb 1%)
	arr2 := make([]int64, N)
	copy(arr2, arr1)
	for i := 0; i < N/100; i++ {
		idx := int(r.Next() % N)
		arr2[idx] += int64(r.Next()%1000 - 500)
	}
	sort.Slice(arr2, func(i, j int) bool { return arr2[i] < arr2[j] })
	for i := 0; i < N; i++ {
		checksum = (checksum*31 + arr2[i]) % MODULUS
	}

	// Pass 3: reversed array
	arr3 := make([]int64, N)
	for i := 0; i < N; i++ {
		arr3[i] = arr1[N-1-i]
	}
	sort.Slice(arr3, func(i, j int) bool { return arr3[i] < arr3[j] })
	for i := 0; i < N; i++ {
		checksum = (checksum*31 + arr3[i]) % MODULUS
	}

	result := int(checksum % MODULUS)
	fmt.Printf("%d\n", result)
	return 0
}

// --- 11. vector_growth -----------------------------------------------------

func bench_vector_growth() int {
	const N = 10000000
	v := make([]int, 0) // no pre-allocation
	checksum := 0

	for i := 0; i < N; i++ {
		v = append(v, i)

		if i > 0 && i%100000 == 0 {
			// Partial checksum: sum of last 100 elements
			sum := 0
			start := i - 100
			if start < 0 {
				start = 0
			}
			for j := start; j < i; j++ {
				sum = (sum + v[j]) % MODULUS
			}
			checksum = (checksum + sum) % MODULUS
		}
	}

	// Pop all elements one at a time
	for len(v) > 0 {
		v = v[:len(v)-1]
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 12. graph_bfs ---------------------------------------------------------

func bench_graph_bfs() int {
	const N_NODES = 100000
	const N_EDGES = 1000000

	r := NewLCG()

	// Generate adjacency list
	adj := make([][]int, N_NODES)
	for i := 0; i < N_EDGES; i++ {
		src := int(r.Next() % N_NODES)
		dst := int(r.Next() % N_NODES)
		adj[src] = append(adj[src], dst)
	}

	bfs := func(start int, visited []bool, dist []int) int {
		// Reset visited
		for i := range visited {
			visited[i] = false
		}

		queue := make([]int, 0, N_NODES)
		queue = append(queue, start)
		visited[start] = true
		dist[start] = 0
		checksum := 0

		for len(queue) > 0 {
			u := queue[0]
			queue = queue[1:]
			for _, v := range adj[u] {
				if !visited[v] {
					visited[v] = true
					dist[v] = dist[u] + 1
					checksum = (checksum + (v*dist[v])%MODULUS) % MODULUS
					queue = append(queue, v)
				}
			}
		}
		return checksum
	}

	// Shared scratch space (reused across BFS calls)
	visited := make([]bool, N_NODES)
	dist := make([]int, N_NODES)

	checksum := bfs(0, visited, dist)

	// BFS from 10 random start nodes
	for i := 0; i < 10; i++ {
		start := int(r.Next() % N_NODES)
		checksum = (checksum + bfs(start, visited, dist)) % MODULUS
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// ============================================================================
// TIER 3: MEMORY & ALLOCATION
// ============================================================================

// --- 15. arena_vs_malloc (sync.Pool as arena) ------------------------------

type arenaObject struct {
	id    int
	value int
	score float64
}

func bench_arena_vs_malloc() int {
	const N_OBJECTS = 100000
	const N_ROUNDS = 10

	arenaChecksum := 0
	mallocChecksum := 0

	r := NewLCG()

	// Arena path: sync.Pool
	pool := &sync.Pool{
		New: func() interface{} {
			return &arenaObject{}
		},
	}

	for round := 0; round < N_ROUNDS; round++ {
		// Allocate N_OBJECTS from pool
		arenaObjs := make([]*arenaObject, N_OBJECTS)
		for i := 0; i < N_OBJECTS; i++ {
			obj := pool.Get().(*arenaObject)
			obj.id = i
			obj.value = int(r.Next())
			obj.score = float64(r.Next()) * 0.001
			arenaObjs[i] = obj
		}

		// Accumulate checksum
		for _, obj := range arenaObjs {
			arenaChecksum = (arenaChecksum + obj.value) % MODULUS
		}

		// Return to pool
		for _, obj := range arenaObjs {
			pool.Put(obj)
		}
	}

	// Malloc path: individual allocations
	for round := 0; round < N_ROUNDS; round++ {
		mallocObjs := make([]*arenaObject, N_OBJECTS)
		for i := 0; i < N_OBJECTS; i++ {
			obj := &arenaObject{
				id:    i,
				value: int(r.Next()),
				score: float64(r.Next()) * 0.001,
			}
			mallocObjs[i] = obj
		}

		for _, obj := range mallocObjs {
			mallocChecksum = (mallocChecksum + obj.value) % MODULUS
		}
		// Objects are GC'd after this block
	}

	result := (arenaChecksum + mallocChecksum) % MODULUS
	fmt.Printf("%d\n", result)
	return 0
}

// --- 16. cache_march -------------------------------------------------------

func bench_cache_march() int {
	const NUM_INTS = 33554432 // 128 MB as int32
	r := NewLCG()

	// Initialize buffer with deterministic data
	buf := make([]int32, NUM_INTS)
	for i := 0; i < NUM_INTS; i++ {
		buf[i] = int32(r.Next())
	}

	totalSum := int64(0)

	// Pass 1: sequential
	for i := 0; i < NUM_INTS; i++ {
		totalSum += int64(buf[i])
	}

	// Pass 2: stride-8
	for i := 0; i < NUM_INTS; i += 8 {
		totalSum += int64(buf[i])
	}

	// Pass 3: stride-64
	for i := 0; i < NUM_INTS; i += 64 {
		totalSum += int64(buf[i])
	}

	// Pass 4: random access (N/100 iterations)
	r = NewLCG()
	nRandom := NUM_INTS / 100
	for i := 0; i < nRandom; i++ {
		idx := r.Next() % uint64(NUM_INTS)
		totalSum += int64(buf[idx])
	}

	result := int(totalSum % MODULUS)
	fmt.Printf("%d\n", result)
	return 0
}

// ============================================================================
// TIER 4: CONCURRENCY & PARALLELISM
// ============================================================================

// --- 18. parallel_reduce ---------------------------------------------------

func bench_parallel_reduce() int {
	const N = 100000000
	nThreads := runtime.NumCPU()

	r := NewLCG()
	data := make([]int64, N)
	for i := 0; i < N; i++ {
		data[i] = int64(r.Next())
	}

	chunkSize := N / nThreads
	results := make(chan int64, nThreads)

	var wg sync.WaitGroup
	wg.Add(nThreads)

	for t := 0; t < nThreads; t++ {
		start := t * chunkSize
		end := start + chunkSize
		if t == nThreads-1 {
			end = N
		}
		go func(s, e int) {
			defer wg.Done()
			var partial int64
			for i := s; i < e; i++ {
				partial += data[i]
			}
			results <- partial
		}(start, end)
	}

	wg.Wait()
	close(results)

	var total int64
	for partial := range results {
		total += partial
	}

	checksum := int(total % MODULUS)
	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 19. mutex_contention --------------------------------------------------

func bench_mutex_contention() int {
	nThreads := runtime.NumCPU()
	const N_INCREMENTS = 1000000

	var counter int64
	var wg sync.WaitGroup
	wg.Add(nThreads)

	for t := 0; t < nThreads; t++ {
		go func() {
			defer wg.Done()
			for i := 0; i < N_INCREMENTS; i++ {
				atomic.AddInt64(&counter, 1)
			}
		}()
	}

	wg.Wait()

	expected := int64(nThreads * N_INCREMENTS)
	if counter != expected {
		fmt.Fprintf(os.Stderr, "mutex_contention: counter=%d expected=%d\n", counter, expected)
		return 1
	}

	result := int(counter % MODULUS)
	fmt.Printf("%d\n", result)
	return 0
}

// --- 20. spsc_queue --------------------------------------------------------

func bench_spsc_queue() int {
	const N_ITEMS = 10000000
	ch := make(chan int, 1024)

	checksum := 0
	var wg sync.WaitGroup
	wg.Add(1)

	// Consumer
	go func() {
		defer wg.Done()
		for val := range ch {
			checksum = (checksum*31 + val) % MODULUS
		}
	}()

	// Producer
	for i := 0; i < N_ITEMS; i++ {
		ch <- i
	}
	close(ch)

	wg.Wait()

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 21. mpmc_queue --------------------------------------------------------

func bench_mpmc_queue() int {
	const N_PRODUCERS = 4
	const N_CONSUMERS = 4
	const N_ITEMS = 10000000
	itemsPerProducer := N_ITEMS / N_PRODUCERS

	ch := make(chan int, 4096)

	var producerWg sync.WaitGroup
	producerWg.Add(N_PRODUCERS)

	for p := 0; p < N_PRODUCERS; p++ {
		go func(base int) {
			defer producerWg.Done()
			for i := 0; i < itemsPerProducer; i++ {
				ch <- base + i
			}
		}(p * itemsPerProducer)
	}

	// Consumer accumulators (atomic for safety, though sequential within each consumer)
	var consumerSums [N_CONSUMERS]int64
	var consumerWg sync.WaitGroup
	consumerWg.Add(N_CONSUMERS)

	for c := 0; c < N_CONSUMERS; c++ {
		go func(idx int) {
			defer consumerWg.Done()
			var local int64
			for val := range ch {
				local += int64(val)
			}
			atomic.StoreInt64(&consumerSums[idx], local)
		}(c)
	}

	producerWg.Wait()
	close(ch)
	consumerWg.Wait()

	var total int64
	for c := 0; c < N_CONSUMERS; c++ {
		total += atomic.LoadInt64(&consumerSums[c])
	}

	checksum := int(total % MODULUS)
	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 22. actor_spam --------------------------------------------------------

func bench_actor_spam() int {
	const N_ACTORS = 10000
	const N_MESSAGES_PER_ACTOR = 100

	type mailbox struct {
		ch   chan int
		done chan int
	}

	actors := make([]mailbox, N_ACTORS)
	for a := 0; a < N_ACTORS; a++ {
		actors[a] = mailbox{
			ch:   make(chan int, N_MESSAGES_PER_ACTOR),
			done: make(chan int, 1),
		}
	}

	// Start actor goroutines
	for a := 0; a < N_ACTORS; a++ {
		go func(mb mailbox) {
			sum := 0
			for m := 0; m < N_MESSAGES_PER_ACTOR; m++ {
				val := <-mb.ch
				sum = (sum + val) % MODULUS
			}
			mb.done <- sum
		}(actors[a])
	}

	// Send messages to all actors
	for a := 0; a < N_ACTORS; a++ {
		for m := 0; m < N_MESSAGES_PER_ACTOR; m++ {
			actors[a].ch <- a*N_MESSAGES_PER_ACTOR + m
		}
	}

	// Collect results
	totalSum := 0
	for a := 0; a < N_ACTORS; a++ {
		sum := <-actors[a].done
		totalSum = (totalSum + sum) % MODULUS
	}

	fmt.Printf("%d\n", totalSum)
	return 0
}

// --- 23. async_ready_pipeline ----------------------------------------------

func bench_async_ready_pipeline() int {
	const N_FUTURES = 1000
	const N_ROUNDS = 10000

	checksum := 0

	for round := 0; round < N_ROUNDS; round++ {
		results := make(chan int, N_FUTURES)

		// Spawn goroutines that return immediately-ready "futures"
		for f := 0; f < N_FUTURES; f++ {
			go func(id int) {
				results <- (id * 7) % MODULUS
			}(f)
		}

		// "Await" all futures
		for f := 0; f < N_FUTURES; f++ {
			result := <-results
			checksum = (checksum + result) % MODULUS
		}
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// ============================================================================
// TIER 5: IO & SYSTEMS
// ============================================================================

// --- 24. file_read_streaming -----------------------------------------------

func bench_file_read_streaming() int {
	const FILE_SIZE int64 = 1 << 30 // 1 GB
	const CHUNK_SIZE = 65536

	// Create temp file with deterministic data
	f, err := os.CreateTemp("", "bench_read_*.tmp")
	if err != nil {
		fmt.Fprintf(os.Stderr, "file_read_streaming: create temp: %v\n", err)
		return 1
	}
	tmpPath := f.Name()
	defer os.Remove(tmpPath)

	r := NewLCG()
	chunk := make([]byte, CHUNK_SIZE)
	var written int64
	for written < FILE_SIZE {
		for i := range chunk {
			chunk[i] = byte(r.Next() & 0xFF)
		}
		toWrite := chunk
		if remaining := FILE_SIZE - written; remaining < int64(len(chunk)) {
			toWrite = chunk[:remaining]
		}
		n, err := f.Write(toWrite)
		if err != nil {
			fmt.Fprintf(os.Stderr, "file_read_streaming: write: %v\n", err)
			f.Close()
			return 1
		}
		written += int64(n)
	}
	f.Close()

	// Re-open for reading
	f, err = os.Open(tmpPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "file_read_streaming: open: %v\n", err)
		return 1
	}
	defer f.Close()

	checksum := 0
	buf := make([]byte, CHUNK_SIZE)
	for {
		n, err := f.Read(buf)
		if n > 0 {
			sum := 0
			for i := 0; i < n; i++ {
				sum += int(buf[i])
			}
			checksum = (checksum*31 + sum) % MODULUS
		}
		if err == io.EOF {
			break
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "file_read_streaming: read: %v\n", err)
			return 1
		}
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 25. file_write_streaming ----------------------------------------------

func bench_file_write_streaming() int {
	const FILE_SIZE int64 = 1 << 30 // 1 GB
	const CHUNK_SIZE = 65536
	const FSYNC_INTERVAL int64 = 16 << 20 // 16 MB

	f, err := os.CreateTemp("", "bench_write_*.tmp")
	if err != nil {
		fmt.Fprintf(os.Stderr, "file_write_streaming: create temp: %v\n", err)
		return 1
	}
	tmpPath := f.Name()
	defer os.Remove(tmpPath)

	r := NewLCG()
	chunk := make([]byte, CHUNK_SIZE)
	checksum := 0
	var written int64
	nextFsync := FSYNC_INTERVAL

	for written < FILE_SIZE {
		// Fill chunk with deterministic data
		sum := 0
		for i := range chunk {
			b := byte(r.Next() & 0xFF)
			chunk[i] = b
			sum += int(b)
		}
		checksum = (checksum*31 + sum) % MODULUS

		toWrite := chunk
		if remaining := FILE_SIZE - written; remaining < int64(len(chunk)) {
			toWrite = chunk[:remaining]
		}
		n, err := f.Write(toWrite)
		if err != nil {
			fmt.Fprintf(os.Stderr, "file_write_streaming: write: %v\n", err)
			f.Close()
			return 1
		}
		written += int64(n)

		// fsync at specified intervals
		if written >= nextFsync {
			if err := f.Sync(); err != nil {
				fmt.Fprintf(os.Stderr, "file_write_streaming: fsync: %v\n", err)
				f.Close()
				return 1
			}
			nextFsync += FSYNC_INTERVAL
		}
	}

	f.Close()

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 26. tcp_echo_throughput ------------------------------------------------

func bench_tcp_echo_throughput() int {
	const N_ROUNDTRIPS = 5000
	const PAYLOAD_SIZE = 65536

	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintf(os.Stderr, "tcp_echo: listen: %v\n", err)
		return 1
	}

	errCh := make(chan error, 1)

	// Server goroutine
	go func() {
		conn, err := ln.Accept()
		if err != nil {
			errCh <- err
			return
		}
		defer conn.Close()

		buf := make([]byte, PAYLOAD_SIZE)
		for i := 0; i < N_ROUNDTRIPS; i++ {
			if _, err := io.ReadFull(conn, buf); err != nil {
				errCh <- err
				return
			}
			if _, err := conn.Write(buf); err != nil {
				errCh <- err
				return
			}
		}
		errCh <- nil
	}()

	conn, err := net.Dial("tcp", ln.Addr().String())
	if err != nil {
		fmt.Fprintf(os.Stderr, "tcp_echo: dial: %v\n", err)
		return 1
	}
	defer conn.Close()

	// Deterministic payload
	r := NewLCG()
	payload := make([]byte, PAYLOAD_SIZE)
	for i := 0; i < PAYLOAD_SIZE; i++ {
		payload[i] = byte(r.Next() & 0xFF)
	}
	recvBuf := make([]byte, PAYLOAD_SIZE)

	checksum := 0
	for i := 0; i < N_ROUNDTRIPS; i++ {
		if _, err := conn.Write(payload); err != nil {
			fmt.Fprintf(os.Stderr, "tcp_echo: write: %v\n", err)
			return 1
		}
		if _, err := io.ReadFull(conn, recvBuf); err != nil {
			fmt.Fprintf(os.Stderr, "tcp_echo: read: %v\n", err)
			return 1
		}

		// Verify payload integrity
		for j := 0; j < PAYLOAD_SIZE; j++ {
			if recvBuf[j] != payload[j] {
				fmt.Fprintf(os.Stderr, "tcp_echo: mismatch at round %d byte %d\n", i, j)
				return 1
			}
		}

		checksum = (checksum*31 + i) % MODULUS
	}

	conn.Close()
	ln.Close()

	if err := <-errCh; err != nil {
		fmt.Fprintf(os.Stderr, "tcp_echo: server error: %v\n", err)
		return 1
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// --- 27. process_spawn_chain ------------------------------------------------

func bench_process_spawn_chain() int {
	const N_SPAWNS = 1000
	checksum := 0

	for i := 0; i < N_SPAWNS; i++ {
		val := strconv.Itoa(i)
		var cmd *exec.Cmd
		if runtime.GOOS == "windows" {
			cmd = exec.Command("cmd", "/c", "echo", val)
		} else {
			cmd = exec.Command("echo", val)
		}
		out, err := cmd.Output()
		if err != nil {
			fmt.Fprintf(os.Stderr, "process_spawn_chain: exec: %v\n", err)
			return 1
		}
		// Trim whitespace/newline
		s := strings.TrimSpace(string(out))
		parsed, err := strconv.Atoi(s)
		if err != nil {
			fmt.Fprintf(os.Stderr, "process_spawn_chain: parse '%s': %v\n", s, err)
			return 1
		}
		checksum = (checksum*31 + parsed) % MODULUS
	}

	fmt.Printf("%d\n", checksum)
	return 0
}

// ============================================================================
// TIER 7: COMPILER QUALITY
// ============================================================================

// --- 30. build_self_stress --------------------------------------------------

func bench_build_self_stress() int {
	exePath, err := os.Executable()
	if err != nil {
		fmt.Fprintf(os.Stderr, "build_self_stress: %v\n", err)
		return 1
	}
	info, err := os.Stat(exePath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "build_self_stress: stat %s: %v\n", exePath, err)
		return 1
	}
	checksum := int(info.Size() % MODULUS)
	fmt.Printf("%d\n", checksum)
	return 0
}

// ============================================================================
// DISPATCHER
// ============================================================================

func main() {
	if len(os.Args) < 2 {
		fmt.Println("usage: bench <benchmark_name>")
		os.Exit(1)
	}

	name := os.Args[1]
	var exitCode int

	switch name {
	// Tier 1: Compute
	case "binary_trees":
		exitCode = bench_binary_trees()
	case "mandelbrot":
		exitCode = bench_mandelbrot()
	case "fasta":
		exitCode = bench_fasta()
	case "regex_redux":
		exitCode = bench_regex_redux()
	case "pidigits":
		exitCode = bench_pidigits()

	// Tier 2: Data Structures
	case "hashmap_heavy":
		exitCode = bench_hashmap_heavy()
	case "btree_scan":
		exitCode = bench_btree_scan()
	case "sort_gauntlet":
		exitCode = bench_sort_gauntlet()
	case "vector_growth":
		exitCode = bench_vector_growth()
	case "graph_bfs":
		exitCode = bench_graph_bfs()

	// Tier 3: Memory
	case "arena_vs_malloc":
		exitCode = bench_arena_vs_malloc()
	case "cache_march":
		exitCode = bench_cache_march()

	// Tier 4: Concurrency
	case "parallel_reduce":
		exitCode = bench_parallel_reduce()
	case "mutex_contention":
		exitCode = bench_mutex_contention()
	case "spsc_queue":
		exitCode = bench_spsc_queue()
	case "mpmc_queue":
		exitCode = bench_mpmc_queue()
	case "actor_spam":
		exitCode = bench_actor_spam()
	case "async_ready_pipeline":
		exitCode = bench_async_ready_pipeline()

	// Tier 5: IO
	case "file_read_streaming":
		exitCode = bench_file_read_streaming()
	case "file_write_streaming":
		exitCode = bench_file_write_streaming()
	case "tcp_echo_throughput":
		exitCode = bench_tcp_echo_throughput()
	case "process_spawn_chain":
		exitCode = bench_process_spawn_chain()

	// Tier 7: Compiler Quality
	case "build_self_stress":
		exitCode = bench_build_self_stress()

	// Skipped benchmarks (documented stubs that return checksum=0 signature)
	case "nbody":
		fmt.Println("SKIPPED: nbody — heavy FP, contract allows skip")
		exitCode = 1
	case "spectral_norm":
		fmt.Println("SKIPPED: spectral_norm — heavy FP, contract allows skip")
		exitCode = 1
	case "alloc_small_churn":
		fmt.Println("SKIPPED: alloc_small_churn — Go is GC'd, manual alloc misrepresentative")
		exitCode = 1
	case "alloc_large_objects":
		fmt.Println("SKIPPED: alloc_large_objects — Go is GC'd, manual alloc misrepresentative")
		exitCode = 1
	case "rc_vs_gc_trace":
		fmt.Println("SKIPPED: rc_vs_gc_trace — Go has no Rc/Arc in stdlib")
		exitCode = 1
	case "c_ffi_call_hotloop":
		fmt.Println("SKIPPED: c_ffi_call_hotloop — requires CGo (cgo disabled for pure-Go build)")
		exitCode = 1
	case "c_buffer_handoff":
		fmt.Println("SKIPPED: c_buffer_handoff — requires CGo (cgo disabled for pure-Go build)")
		exitCode = 1

	default:
		fmt.Printf("unknown benchmark: %s\n", name)
		os.Exit(1)
	}

	os.Exit(exitCode)
}
