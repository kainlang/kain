! =============================================================================
!  bench.f95 — CASES_V3 Fortran God File
!  =============================================================================
!
!  COMPILE:
!    gfortran -O3 -march=native bench.f95 c_ffi.c -o bench
!
!  RUN:
!    ./bench <benchmark_name>
!    ./bench --compute-all
!
!  IMPLEMENTS 16 benchmarks:
!    Tier 1 (Compute): binary_trees, nbody, spectral_norm, mandelbrot, fasta, pidigits
!    Tier 2 (Data Struct): sort_gauntlet, vector_growth
!    Tier 3 (Memory): alloc_small_churn, alloc_large_objects, arena_vs_malloc, cache_march
!    Tier 5 (IO): file_read_streaming, file_write_streaming
!    Tier 6 (FFI): c_ffi_call_hotloop
!    Tier 7 (Build): build_self_stress
!
!  Based on the C++ reference implementation.
! =============================================================================

program cas3_fortran
    use, intrinsic :: iso_fortran_env, only: int8, int32, int64, real64
    use, intrinsic :: iso_c_binding, only: c_int64_t
    implicit none

    ! ==========================================================================
    !  CONSTANTS
    ! ==========================================================================

    integer(int64), parameter :: RANDOM_SEED = 42_int64
    integer(int64), parameter :: MODULUS = 1000000007_int64

    ! ==========================================================================
    !  EXPECTED VALUES - set to 0 initially; calibrate via --compute-all
    ! ==========================================================================

    integer(int64), parameter :: BINARY_TREES_EXPECTED        = 0
    integer(int64), parameter :: NBODY_EXPECTED               = 0
    integer(int64), parameter :: SPECTRAL_NORM_EXPECTED       = 0
    integer(int64), parameter :: MANDELBROT_EXPECTED          = 0
    integer(int64), parameter :: FASTA_EXPECTED               = 0
    integer(int64), parameter :: PIDIGITS_EXPECTED            = 0
    integer(int64), parameter :: SORT_GAUNTLET_EXPECTED       = 0
    integer(int64), parameter :: VECTOR_GROWTH_EXPECTED       = 0
    integer(int64), parameter :: ALLOC_SMALL_CHURN_EXPECTED   = 0
    integer(int64), parameter :: ALLOC_LARGE_OBJECTS_EXPECTED = 0
    integer(int64), parameter :: ARENA_VS_MALLOC_EXPECTED     = 0
    integer(int64), parameter :: CACHE_MARCH_EXPECTED         = 0
    integer(int64), parameter :: FILE_READ_EXPECTED           = 0
    integer(int64), parameter :: FILE_WRITE_EXPECTED          = 0
    integer(int64), parameter :: C_FFI_HOTLOOP_EXPECTED       = 0
    integer(int64), parameter :: BUILD_SELF_STRESS_EXPECTED   = 0

    ! ==========================================================================
    !  TYPES
    ! ==========================================================================

    type :: TreeNode
        integer(int64) :: value
        type(TreeNode), pointer :: left => null()
        type(TreeNode), pointer :: right => null()
    end type TreeNode

    type :: ArenaObj
        integer(int64) :: id
        integer(int64) :: value
        real(real64)   :: score
    end type ArenaObj

    type :: Body
        real(real64) :: x, y, z
        real(real64) :: vx, vy, vz
        real(real64) :: mass
    end type Body

    ! ==========================================================================
    !  INTERFACE — C function for c_ffi_call_hotloop
    ! ==========================================================================

    interface
        integer(c_int64_t) function c_add(a, b) bind(c, name="c_add")
            use, intrinsic :: iso_c_binding, only: c_int64_t
            implicit none
            integer(c_int64_t), value :: a, b
        end function c_add
    end interface

    ! ==========================================================================
    !  LOCAL VARIABLES
    ! ==========================================================================

    character(len=64) :: cmd
    integer :: n_args, name_len, ierr
    integer(int64) :: result

    ! ==========================================================================
    !  COMMAND LINE PARSING
    ! ==========================================================================

    n_args = command_argument_count()
    if (n_args < 1) then
        write(0, '(a)') "usage: bench <benchmark_name>"
        write(0, '(a)') "       bench --compute-all"
        stop 1
    end if

    call get_command_argument(1, cmd, name_len, ierr)
    if (ierr /= 0) then
        write(0, '(a)') "Error reading command argument"
        stop 1
    end if
    cmd = trim(cmd)

    ! ==========================================================================
    !  DISPATCHER
    ! ==========================================================================

    if (cmd == "--compute-all") then
        call compute_all()
        stop 0
    end if

    ! Tier 1: Compute
    if (cmd == "binary_trees") then
        result = bench_binary_trees()
        call check_exit("binary_trees", result, BINARY_TREES_EXPECTED)
    else if (cmd == "nbody") then
        result = bench_nbody()
        call check_exit("nbody", result, NBODY_EXPECTED)
    else if (cmd == "spectral_norm") then
        result = bench_spectral_norm()
        call check_exit("spectral_norm", result, SPECTRAL_NORM_EXPECTED)
    else if (cmd == "mandelbrot") then
        result = bench_mandelbrot()
        call check_exit("mandelbrot", result, MANDELBROT_EXPECTED)
    else if (cmd == "fasta") then
        result = bench_fasta()
        call check_exit("fasta", result, FASTA_EXPECTED)
    else if (cmd == "pidigits") then
        result = bench_pidigits()
        call check_exit("pidigits", result, PIDIGITS_EXPECTED)
    ! Tier 2: Data Structures
    else if (cmd == "sort_gauntlet") then
        result = bench_sort_gauntlet()
        call check_exit("sort_gauntlet", result, SORT_GAUNTLET_EXPECTED)
    else if (cmd == "vector_growth") then
        result = bench_vector_growth()
        call check_exit("vector_growth", result, VECTOR_GROWTH_EXPECTED)
    ! Tier 3: Memory
    else if (cmd == "alloc_small_churn") then
        result = bench_alloc_small_churn()
        call check_exit("alloc_small_churn", result, ALLOC_SMALL_CHURN_EXPECTED)
    else if (cmd == "alloc_large_objects") then
        result = bench_alloc_large_objects()
        call check_exit("alloc_large_objects", result, ALLOC_LARGE_OBJECTS_EXPECTED)
    else if (cmd == "arena_vs_malloc") then
        result = bench_arena_vs_malloc()
        call check_exit("arena_vs_malloc", result, ARENA_VS_MALLOC_EXPECTED)
    else if (cmd == "cache_march") then
        result = bench_cache_march()
        call check_exit("cache_march", result, CACHE_MARCH_EXPECTED)
    ! Tier 5: IO
    else if (cmd == "file_read_streaming") then
        result = bench_file_read_streaming()
        call check_exit("file_read_streaming", result, FILE_READ_EXPECTED)
    else if (cmd == "file_write_streaming") then
        result = bench_file_write_streaming()
        call check_exit("file_write_streaming", result, FILE_WRITE_EXPECTED)
    ! Tier 6: FFI
    else if (cmd == "c_ffi_call_hotloop") then
        result = bench_c_ffi_call_hotloop()
        call check_exit("c_ffi_call_hotloop", result, C_FFI_HOTLOOP_EXPECTED)
    ! Tier 7: Build
    else if (cmd == "build_self_stress") then
        result = bench_build_self_stress()
        call check_exit("build_self_stress", result, BUILD_SELF_STRESS_EXPECTED)
    else
        write(0, '(a,a)') "unknown benchmark: ", trim(cmd)
        stop 1
    end if

contains

    ! ==========================================================================
    !  HELPERS
    ! ==========================================================================

    ! Deterministic LCG
    function lcg_next(state) result(r)
        integer(int64), intent(inout) :: state
        integer(int64) :: r
        state = iand(state * 1103515245_int64 + 12345_int64, 2147483647_int64)
        r = state
    end function lcg_next

    ! Safe modulo
    function safe_mod(v, m) result(r)
        integer(int64), intent(in) :: v, m
        integer(int64) :: r
        r = mod(v, m)
        if (r < 0) r = r + m
    end function safe_mod

    ! Check result and exit
    subroutine check_exit(name, result, expected)
        character(len=*), intent(in) :: name
        integer(int64), intent(in) :: result, expected
        if (result == expected) then
            stop 0
        else
            write(0, '(a,a,a,i0,a,i0)') "[FAIL] ", trim(name), ": got ", result, &
                  ", expected ", expected
            stop 1
        end if
    end subroutine check_exit

    ! Print result
    subroutine print_result(name, result)
        character(len=*), intent(in) :: name
        integer(int64), intent(in) :: result
        write(*, '(a,a,a,i0)') "  ", trim(name), " => ", result
    end subroutine print_result

    ! ==========================================================================
    !  COMPUTE-ALL MODE
    ! ==========================================================================

    subroutine compute_all()
        write(*, '(a)') "=== CASES_V3 Fortran Expected Values ==="
        call print_result("binary_trees", bench_binary_trees())
        call print_result("nbody", bench_nbody())
        call print_result("spectral_norm", bench_spectral_norm())
        call print_result("mandelbrot", bench_mandelbrot())
        call print_result("fasta", bench_fasta())
        call print_result("pidigits", bench_pidigits())
        call print_result("sort_gauntlet", bench_sort_gauntlet())
        call print_result("vector_growth", bench_vector_growth())
        call print_result("alloc_small_churn", bench_alloc_small_churn())
        call print_result("alloc_large_objects", bench_alloc_large_objects())
        call print_result("arena_vs_malloc", bench_arena_vs_malloc())
        call print_result("cache_march", bench_cache_march())
        call print_result("file_read_streaming", bench_file_read_streaming())
        call print_result("file_write_streaming", bench_file_write_streaming())
        call print_result("c_ffi_call_hotloop", bench_c_ffi_call_hotloop())
        call print_result("build_self_stress", bench_build_self_stress())
        write(*, '(a)') "=== End ==="
    end subroutine compute_all

    ! ==========================================================================
    !  TREE HELPERS (for binary_trees)
    ! ==========================================================================

    recursive function alloc_tree(depth) result(node)
        integer(int64), intent(in) :: depth
        type(TreeNode), pointer :: node
        if (depth <= 0) then
            node => null()
            return
        end if
        allocate(node)
        node%value = 1_int64
        node%left => alloc_tree(depth - 1)
        node%right => alloc_tree(depth - 1)
    end function alloc_tree

    recursive function tree_sum(node) result(sum)
        type(TreeNode), pointer, intent(in) :: node
        integer(int64) :: sum
        if (.not. associated(node)) then
            sum = 0
            return
        end if
        sum = node%value + tree_sum(node%left) + tree_sum(node%right)
    end function tree_sum

    recursive subroutine free_tree(node)
        type(TreeNode), pointer :: node
        if (.not. associated(node)) return
        call free_tree(node%left)
        call free_tree(node%right)
        deallocate(node)
    end subroutine free_tree

    ! ==========================================================================
    !  TIER 1: COMPUTE & ALGORITHM
    ! ==========================================================================

    ! --------------------------------------------------------------------------
    !  1. binary_trees
    ! --------------------------------------------------------------------------

    function bench_binary_trees() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: MIN_DEPTH = 4, MAX_DEPTH = 18
        integer(int64) :: d, iterations, i, s
        type(TreeNode), pointer :: tree

        checksum = 0_int64
        d = MIN_DEPTH
        do while (d <= MAX_DEPTH)
            iterations = ishft(1_int64, int(MAX_DEPTH - d + MIN_DEPTH, int64))
            do i = 1, iterations
                tree => alloc_tree(d)
                s = tree_sum(tree)
                checksum = safe_mod(checksum + s, MODULUS)
                call free_tree(tree)
            end do
            d = d + 2
        end do
    end function bench_binary_trees

    ! --------------------------------------------------------------------------
    !  2. nbody
    ! --------------------------------------------------------------------------

    function bench_nbody() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N_BODIES = 500, TIMESTEPS = 100
        real(real64), parameter :: DT = 0.01_real64, SOFTENING = 1e-9_real64
        type(Body), allocatable :: bodies(:)
        integer(int64) :: i, j, t, state
        real(real64) :: fx, fy, fz, dx, dy, dz, dist, inv_dist3, total

        state = RANDOM_SEED
        allocate(bodies(N_BODIES))
        do i = 1, N_BODIES
            bodies(i)%x    = (real(lcg_next(state), real64) / 1e6_real64) - 500.0_real64
            bodies(i)%y    = (real(lcg_next(state), real64) / 1e6_real64) - 500.0_real64
            bodies(i)%z    = (real(lcg_next(state), real64) / 1e6_real64) - 500.0_real64
            bodies(i)%vx   = (real(lcg_next(state), real64) / 1e9_real64) - 0.5_real64
            bodies(i)%vy   = (real(lcg_next(state), real64) / 1e9_real64) - 0.5_real64
            bodies(i)%vz   = (real(lcg_next(state), real64) / 1e9_real64) - 0.5_real64
            bodies(i)%mass = 1.0_real64 + (real(lcg_next(state), real64) / 1e9_real64)
        end do
        do t = 1, TIMESTEPS
            do i = 1, N_BODIES
                fx = 0.0_real64; fy = 0.0_real64; fz = 0.0_real64
                do j = 1, N_BODIES
                    if (i == j) cycle
                    dx = bodies(i)%x - bodies(j)%x
                    dy = bodies(i)%y - bodies(j)%y
                    dz = bodies(i)%z - bodies(j)%z
                    dist = sqrt(dx*dx + dy*dy + dz*dz + SOFTENING)
                    inv_dist3 = 1.0_real64 / (dist * dist * dist)
                    fx = fx - dx * bodies(j)%mass * inv_dist3
                    fy = fy - dy * bodies(j)%mass * inv_dist3
                    fz = fz - dz * bodies(j)%mass * inv_dist3
                end do
                bodies(i)%vx = bodies(i)%vx + fx * DT
                bodies(i)%vy = bodies(i)%vy + fy * DT
                bodies(i)%vz = bodies(i)%vz + fz * DT
            end do
            do i = 1, N_BODIES
                bodies(i)%x = bodies(i)%x + bodies(i)%vx * DT
                bodies(i)%y = bodies(i)%y + bodies(i)%vy * DT
                bodies(i)%z = bodies(i)%z + bodies(i)%vz * DT
            end do
        end do
        total = 0.0_real64
        do i = 1, N_BODIES
            total = total + bodies(i)%x + bodies(i)%y + bodies(i)%z
        end do
        checksum = safe_mod(int(floor(total), int64), MODULUS)
        deallocate(bodies)
    end function bench_nbody

    ! --------------------------------------------------------------------------
    !  3. spectral_norm
    ! --------------------------------------------------------------------------

    function spectral_a(i, j) result(val)
        integer(int64), intent(in) :: i, j
        real(real64) :: val
        integer(int64) :: s
        s = (i + j - 2) * (i + j - 1) / 2 + i
        val = 1.0_real64 / real(s, real64)
    end function spectral_a

    function bench_spectral_norm() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N = 2000
        real(real64), allocatable :: u(:), v(:)
        integer(int64) :: iter, i, j

        allocate(u(N), v(N))
        u = 1.0_real64
        v = 0.0_real64
        do iter = 1, 10
            do i = 1, N
                v(i) = 0.0_real64
                do j = 1, N
                    v(i) = v(i) + u(j) * spectral_a(i, j)
                end do
            end do
            do i = 1, N
                u(i) = 0.0_real64
                do j = 1, N
                    u(i) = u(i) + v(j) * spectral_a(j, i)
                end do
            end do
        end do
        checksum = safe_mod( &
            int(floor(sqrt(dot_product(u, v) / dot_product(v, v)) * 1e9_real64), int64), &
            MODULUS)
        deallocate(u, v)
    end function bench_spectral_norm

    ! --------------------------------------------------------------------------
    !  4. mandelbrot
    ! --------------------------------------------------------------------------

    function bench_mandelbrot() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: WIDTH = 800, HEIGHT = 800, MAX_ITER = 200
        real(real64), parameter :: XMIN = -2.0_real64, XMAX = 1.0_real64
        real(real64), parameter :: YMIN = -1.5_real64, YMAX = 1.5_real64
        integer(int64) :: x, y, iter
        real(real64) :: cr, ci, zr, zi, nzr, nzi

        checksum = 0_int64
        do y = 1, HEIGHT
            ci = YMIN + (YMAX - YMIN) * real(y - 1, real64) / real(HEIGHT, real64)
            do x = 1, WIDTH
                cr = XMIN + (XMAX - XMIN) * real(x - 1, real64) / real(WIDTH, real64)
                zr = 0.0_real64; zi = 0.0_real64; iter = 0
                do while (zr*zr + zi*zi <= 4.0_real64 .and. iter < MAX_ITER)
                    nzr = zr*zr - zi*zi + cr
                    nzi = 2.0_real64 * zr * zi + ci
                    zr = nzr; zi = nzi; iter = iter + 1
                end do
                checksum = checksum + iter
            end do
        end do
        checksum = safe_mod(checksum, MODULUS)
    end function bench_mandelbrot

    ! --------------------------------------------------------------------------
    !  5. fasta
    ! --------------------------------------------------------------------------

    function bench_fasta() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N = 250000
        character(len=*), parameter :: ALU = &
            "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTG" // &
            "AGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAAT" // &
            "TAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTT" // &
            "GAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAG" // &
            "CGAGACTCCGTCTCAAAAA"
        integer(int64) :: freq(0:255)
        integer(int64) :: i, state, r, total_weight
        integer(int64) :: a_count, c_count, g_count, t_count

        freq = 0
        do i = 1, len(ALU)
            freq(iachar(ALU(i:i))) = freq(iachar(ALU(i:i))) + 1
        end do
        a_count = freq(iachar('A'))
        c_count = freq(iachar('C'))
        g_count = freq(iachar('G'))
        t_count = freq(iachar('T'))
        total_weight = a_count + c_count + g_count + t_count

        state = RANDOM_SEED
        checksum = 0_int64
        do i = 1, N
            r = lcg_next(state)
            r = mod(r, total_weight)
            if (r < a_count) then
                checksum = safe_mod(checksum * 31 + iachar('A'), MODULUS)
            else if (r < a_count + c_count) then
                checksum = safe_mod(checksum * 31 + iachar('C'), MODULUS)
            else if (r < a_count + c_count + g_count) then
                checksum = safe_mod(checksum * 31 + iachar('G'), MODULUS)
            else
                checksum = safe_mod(checksum * 31 + iachar('T'), MODULUS)
            end if
        end do
    end function bench_fasta

    ! --------------------------------------------------------------------------
    !  6. pidigits
    ! --------------------------------------------------------------------------

    function arctan_series(x_inv, terms) result(sum)
        real(real64), intent(in) :: x_inv
        integer(int64), intent(in) :: terms
        real(real64) :: sum, x, x2, term
        integer(int64) :: i, divisor
        x = 1.0_real64 / x_inv
        x2 = x * x
        sum = 0.0_real64
        term = x
        do i = 0, terms - 1
            divisor = 2 * i + 1
            if (mod(i, 2) == 0) then
                sum = sum + term / real(divisor, real64)
            else
                sum = sum - term / real(divisor, real64)
            end if
            term = term * x2
            if (abs(term) < 1e-300_real64) exit
        end do
    end function arctan_series

    function bench_pidigits() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N = 5000
        real(real64) :: pi_val
        integer(int64) :: i, digit
        pi_val = 16.0_real64 * arctan_series(5.0_real64, 1000000_int64) &
               - 4.0_real64 * arctan_series(239.0_real64, 1000000_int64)
        checksum = 0_int64
        do i = 1, N
            pi_val = pi_val * 10.0_real64
            digit = int(pi_val, int64)
            pi_val = pi_val - real(digit, real64)
            checksum = safe_mod(checksum * 31 + digit, MODULUS)
        end do
    end function bench_pidigits

    ! ==========================================================================
    !  TIER 2: DATA STRUCTURES
    ! ==========================================================================

    ! --------------------------------------------------------------------------
    !  10. sort_gauntlet
    ! --------------------------------------------------------------------------

    function accumulate_array(a) result(cs)
        integer(int64), intent(in) :: a(:)
        integer(int64) :: cs
        integer(int64) :: i
        cs = 0_int64
        do i = 1, size(a)
            cs = safe_mod(cs * 31 + a(i), MODULUS)
        end do
    end function accumulate_array

    recursive subroutine quicksort(a, lo, hi)
        integer(int64), intent(inout) :: a(:)
        integer(int64), intent(in) :: lo, hi
        integer(int64) :: pivot, temp, i, j, mid
        if (lo >= hi) return
        if (hi - lo <= 20_int64) then
            do i = lo + 1, hi
                temp = a(i)
                j = i - 1
                do while (j >= lo .and. a(j) > temp)
                    a(j+1) = a(j)
                    j = j - 1
                end do
                a(j+1) = temp
            end do
            return
        end if
        mid = (lo + hi) / 2
        if (a(lo) > a(mid)) then; temp = a(lo); a(lo) = a(mid); a(mid) = temp; end if
        if (a(mid) > a(hi)) then; temp = a(mid); a(mid) = a(hi); a(hi) = temp; end if
        if (a(lo) > a(hi)) then; temp = a(lo); a(lo) = a(hi); a(hi) = temp; end if
        pivot = a(mid)
        i = lo; j = hi
        do
            do while (a(i) < pivot); i = i + 1; end do
            do while (a(j) > pivot); j = j - 1; end do
            if (i >= j) exit
            temp = a(i); a(i) = a(j); a(j) = temp
            i = i + 1; j = j - 1
        end do
        call quicksort(a, lo, i - 1)
        call quicksort(a, i, hi)
    end subroutine quicksort

    function bench_sort_gauntlet() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N = 1000000
        integer(int64), allocatable :: arr(:)
        integer(int64) :: i, state

        allocate(arr(N))
        state = RANDOM_SEED
        do i = 1, N
            arr(i) = lcg_next(state)
        end do
        call quicksort(arr, 1_int64, N)
        checksum = accumulate_array(arr)

        do i = 1, N
            if (mod(lcg_next(state), 100_int64) == 0) then
                arr(i) = lcg_next(state)
            end if
        end do
        call quicksort(arr, 1_int64, N)
        checksum = safe_mod(checksum + accumulate_array(arr), MODULUS)

        arr = arr(N:1:-1)
        call quicksort(arr, 1_int64, N)
        checksum = safe_mod(checksum + accumulate_array(arr), MODULUS)
        checksum = safe_mod(checksum, MODULUS)
        deallocate(arr)
    end function bench_sort_gauntlet

    ! --------------------------------------------------------------------------
    !  11. vector_growth
    ! --------------------------------------------------------------------------

    function bench_vector_growth() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N = 10000000
        integer(int64), parameter :: CHECKPOINT_INTERVAL = 100000
        integer(int64), allocatable :: vec(:), tmp(:)
        integer(int64) :: i, partial, j, count, cap

        checksum = 0_int64
        count = 0
        cap = 1024
        allocate(vec(cap))

        do i = 1, N
            count = count + 1
            if (count > cap) then
                cap = cap * 2
                call move_alloc(vec, tmp)
                allocate(vec(cap))
                vec(1:count-1) = tmp(1:count-1)
                deallocate(tmp)
            end if
            vec(count) = i - 1
            if (mod(i, CHECKPOINT_INTERVAL) == 0) then
                partial = 0_int64
                if (count >= 100) then
                    do j = count - 99, count
                        partial = safe_mod(partial + vec(j), MODULUS)
                    end do
                else
                    do j = 1, count
                        partial = safe_mod(partial + vec(j), MODULUS)
                    end do
                end if
                checksum = safe_mod(checksum * 31 + partial, MODULUS)
            end if
        end do
        deallocate(vec)
    end function bench_vector_growth

    ! ==========================================================================
    !  TIER 3: MEMORY
    ! ==========================================================================

    ! --------------------------------------------------------------------------
    !  13. alloc_small_churn
    ! --------------------------------------------------------------------------

    function bench_alloc_small_churn() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N_ALLOCS = 1000000
        integer(int64) :: i, state, sz
        integer(int8), allocatable :: buf(:)
        integer(int8) :: pattern
        integer(int64) :: val

        state = RANDOM_SEED
        checksum = 0_int64
        do i = 1, N_ALLOCS
            sz = 16 + int(mod(lcg_next(state), 240_int64), int64)  ! 16..256 bytes
            allocate(buf(sz))
            pattern = int(mod(i - 1, 256), int8)
            buf = pattern
            ! Read first element as int64 via transfer
            val = transfer(buf(1:8), val)
            checksum = safe_mod(checksum + val, MODULUS)
            deallocate(buf)
        end do
    end function bench_alloc_small_churn

    ! --------------------------------------------------------------------------
    !  14. alloc_large_objects
    ! --------------------------------------------------------------------------

    function bench_alloc_large_objects() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N_LARGE = 1000, N_SMALL = 100000
        integer(int64) :: i, j, large_size, state, local_sum, off, small_count, n_ints
        integer(int8), allocatable :: large_buf(:), small_buf(:)
        integer(int64) :: first_int

        state = RANDOM_SEED
        checksum = 0_int64
        do i = 1, N_LARGE
            ! Large allocation: 1MB + random(0..64MB) in bytes
            large_size = 1024_int64 * 1024 + mod(lcg_next(state), 64_int64 * 1024 * 1024)
            allocate(large_buf(large_size))

            ! Touch every page (4096 bytes)
            off = 1
            do while (off <= large_size)
                large_buf(off) = int(mod(i - 1, 128), int8)
                off = off + 4096
            end do

            ! Fill first 256 bytes deterministically
            do off = 1, min(256_int64, large_size)
                large_buf(off) = int(mod(off - 1 + i - 1, 256), int8)
            end do

            ! Read up to 256 int64 values (2048 bytes) per C++ reference
            n_ints = min(256_int64, large_size / 8)
            local_sum = 0_int64
            do off = 1, n_ints
                first_int = transfer(large_buf((off-1)*8+1:off*8), first_int)
                local_sum = safe_mod(local_sum + first_int, MODULUS)
            end do
            checksum = safe_mod(checksum + local_sum, MODULUS)

            ! Small interleaved allocs (64 bytes each)
            small_count = N_SMALL / N_LARGE
            do j = 1, small_count
                allocate(small_buf(64))
                small_buf = int(mod(j - 1, 256), int8)
                first_int = transfer(small_buf(1:8), first_int)
                checksum = safe_mod(checksum + first_int, MODULUS)
                deallocate(small_buf)
            end do
            deallocate(large_buf)
        end do
    end function bench_alloc_large_objects

    ! --------------------------------------------------------------------------
    !  15. arena_vs_malloc
    ! --------------------------------------------------------------------------

    function bench_arena_vs_malloc() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N_OBJECTS = 100000, N_ROUNDS = 10
        type(ArenaObj), allocatable :: arena_objs(:)
        integer(int64) :: i, round, state, arena_check, malloc_check

        state = RANDOM_SEED
        arena_check = 0_int64
        malloc_check = 0_int64

        do round = 1, N_ROUNDS
            ! Arena path
            allocate(arena_objs(N_OBJECTS))
            do i = 1, N_OBJECTS
                arena_objs(i)%id = i - 1
                arena_objs(i)%value = lcg_next(state)
                arena_objs(i)%score = real(lcg_next(state), real64) / 1e6_real64
                arena_check = safe_mod(arena_check + arena_objs(i)%value, MODULUS)
            end do
            deallocate(arena_objs)

            ! Malloc path (allocate/deallocate per object)
            do i = 1, N_OBJECTS
                block
                    type(ArenaObj), allocatable :: obj
                    allocate(obj)
                    obj%id = i - 1
                    obj%value = lcg_next(state)
                    obj%score = real(lcg_next(state), real64) / 1e6_real64
                    malloc_check = safe_mod(malloc_check + obj%value, MODULUS)
                    deallocate(obj)
                end block
            end do
        end do
        checksum = safe_mod(arena_check + malloc_check, MODULUS)
    end function bench_arena_vs_malloc

    ! --------------------------------------------------------------------------
    !  16. cache_march
    ! --------------------------------------------------------------------------

    function bench_cache_march() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: BUFFER_BYTES = 128_int64 * 1024 * 1024  ! 128 MiB
        integer(int64), parameter :: N_INTS = BUFFER_BYTES / 4  ! int32 elements
        integer(int32), allocatable :: buffer(:)
        integer(int64) :: total_sum, i, state

        allocate(buffer(N_INTS))
        state = RANDOM_SEED
        do i = 1, N_INTS
            buffer(i) = int(lcg_next(state), int32)
        end do

        total_sum = 0_int64

        ! Pass 1: sequential
        do i = 1, N_INTS
            total_sum = total_sum + int(buffer(i), int64)
        end do

        ! Pass 2: stride-8
        do i = 1, N_INTS, 8
            total_sum = total_sum + int(buffer(i), int64)
        end do

        ! Pass 3: stride-64
        do i = 1, N_INTS, 64
            total_sum = total_sum + int(buffer(i), int64)
        end do

        ! Pass 4: random access
        state = RANDOM_SEED
        do i = 1, N_INTS / 100
            total_sum = total_sum + int(buffer(int(mod(lcg_next(state), int(N_INTS, int64))) + 1), int64)
        end do

        deallocate(buffer)
        checksum = safe_mod(total_sum, MODULUS)
    end function bench_cache_march

    ! ==========================================================================
    !  TIER 5: I/O
    ! ==========================================================================

    ! --------------------------------------------------------------------------
    !  24. file_read_streaming
    ! --------------------------------------------------------------------------

    function bench_file_read_streaming() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: FILE_SIZE = 1_int64 * 1024 * 1024 * 1024  ! 1 GiB
        integer(int64), parameter :: CHUNK_ELEMS = 8192 ! 65536 bytes / 8 bytes per int64
        character(len=256) :: path
        integer(int64), allocatable :: buf(:)
        integer(int64) :: state, written, to_write, chunk_sum, i, remaining
        integer :: unit, ios

        path = 'fortran_bench_read.tmp'
        state = RANDOM_SEED
        allocate(buf(CHUNK_ELEMS))

        ! Write deterministic data
        open(newunit=unit, file=trim(path), form='unformatted', access='stream', &
             status='replace', action='write', iostat=ios)
        if (ios /= 0) then; checksum = 1; return; end if

        written = 0
        do while (written < FILE_SIZE)
            to_write = min(CHUNK_ELEMS, (FILE_SIZE - written) / 8)
            do i = 1, to_write
                buf(i) = lcg_next(state)
            end do
            write(unit, pos=written*8+1, iostat=ios) buf(1:to_write)
            if (ios /= 0) exit
            written = written + to_write
        end do
        close(unit)

        ! Read back with rolling checksum
        state = RANDOM_SEED
        checksum = 0_int64
        open(newunit=unit, file=trim(path), form='unformatted', access='stream', &
             status='old', action='read', iostat=ios)
        if (ios /= 0) then; checksum = 1; return; end if

        remaining = FILE_SIZE / 8
        do while (remaining > 0)
            to_write = min(CHUNK_ELEMS, remaining)
            read(unit, iostat=ios) buf(1:to_write)
            if (ios /= 0) exit
            chunk_sum = 0_int64
            do i = 1, to_write
                chunk_sum = chunk_sum + buf(i)
            end do
            checksum = safe_mod(checksum * 31 + chunk_sum, MODULUS)
            remaining = remaining - to_write
        end do
        close(unit, status='delete')
        deallocate(buf)
    end function bench_file_read_streaming

    ! --------------------------------------------------------------------------
    !  25. file_write_streaming
    ! --------------------------------------------------------------------------

    function bench_file_write_streaming() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: FILE_SIZE = 1_int64 * 1024 * 1024 * 1024
        integer(int64), parameter :: CHUNK_ELEMS = 8192
        integer(int64), parameter :: FSYNC_ELEMS = (16_int64 * 1024 * 1024) / 8
        character(len=256) :: path
        integer(int64), allocatable :: buf(:)
        integer(int64) :: state, total_written, since_fsync, to_write, i, remaining
        integer :: unit, ios

        path = 'fortran_bench_write.tmp'
        state = RANDOM_SEED
        checksum = 0_int64
        total_written = 0
        since_fsync = 0
        allocate(buf(CHUNK_ELEMS))

        open(newunit=unit, file=trim(path), form='unformatted', access='stream', &
             status='replace', action='write', iostat=ios)
        if (ios /= 0) then; checksum = 1; return; end if

        remaining = FILE_SIZE / 8
        do while (remaining > 0)
            to_write = min(CHUNK_ELEMS, remaining)
            do i = 1, to_write
                buf(i) = lcg_next(state)
                checksum = safe_mod(checksum * 31 + buf(i), MODULUS)
            end do
            write(unit, iostat=ios) buf(1:to_write)
            if (ios /= 0) exit
            total_written = total_written + to_write * 8
            since_fsync = since_fsync + to_write * 8
            remaining = remaining - to_write
            if (since_fsync >= 16_int64 * 1024 * 1024) then
                flush(unit)
                since_fsync = 0
            end if
        end do

        flush(unit)
        close(unit, status='delete')
        deallocate(buf)
    end function bench_file_write_streaming

    ! ==========================================================================
    !  TIER 6: FFI
    ! ==========================================================================

    ! --------------------------------------------------------------------------
    !  28. c_ffi_call_hotloop
    ! --------------------------------------------------------------------------

    function bench_c_ffi_call_hotloop() result(checksum)
        integer(int64) :: checksum
        integer(int64), parameter :: N_CALLS = 10000000
        integer(int64) :: i
        integer(c_int64_t) :: result_val

        checksum = 0_int64
        do i = 0, N_CALLS - 1
            result_val = c_add(int(i, c_int64_t), int(i + 1, c_int64_t))
            checksum = safe_mod(checksum * 31 + result_val, MODULUS)
        end do
    end function bench_c_ffi_call_hotloop

    ! ==========================================================================
    !  TIER 7: BUILD SELF STRESS
    ! ==========================================================================

    ! --------------------------------------------------------------------------
    !  30. build_self_stress
    ! --------------------------------------------------------------------------

    function bench_build_self_stress() result(checksum)
        integer(int64) :: checksum
        integer :: unit, ios
        integer(int64) :: file_size
        character(len=256) :: exe_path
        logical :: exists

        exe_path = 'bench.exe'
        inquire(file=trim(exe_path), exist=exists, size=file_size, iostat=ios)
        if (.not. exists .or. ios /= 0) then
            exe_path = './bench.exe'
            inquire(file=trim(exe_path), exist=exists, size=file_size, iostat=ios)
        end if
        if (.not. exists .or. ios /= 0) then
            checksum = 0_int64
            return
        end if
        checksum = safe_mod(file_size, MODULUS)
    end function bench_build_self_stress

end program cas3_fortran
