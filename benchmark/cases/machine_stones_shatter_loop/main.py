x = [3, 13, 29, 43, 61, 79, 101, 113]
y = [5, 17, 31, 47, 67, 83, 103, 127]
vx = [7, 19, 37, 53, 71, 89, 107, 131]
vy = [11, 23, 41, 59, 73, 97, 109, 137]
alive = [True, False, True, False, True, False, True, False]

iterations = 500000
expected = -1399052960
acc = 0

for round_index in range(iterations):
    for lane in range(len(x)):
        if alive[lane]:
            acc += (((x[lane] + round_index) % 97) * vx[lane]) + y[lane] + lane
        else:
            acc = acc - (((y[lane] + round_index) % 89) * vy[lane]) + x[lane] - lane

if acc != expected:
    raise SystemExit(1)
