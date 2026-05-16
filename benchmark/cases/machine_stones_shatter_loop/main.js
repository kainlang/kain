const x = [3, 13, 29, 43, 61, 79, 101, 113];
const y = [5, 17, 31, 47, 67, 83, 103, 127];
const vx = [7, 19, 37, 53, 71, 89, 107, 131];
const vy = [11, 23, 41, 59, 73, 97, 109, 137];
const alive = [true, false, true, false, true, false, true, false];

const iterations = 500000;
const expected = -1399052960;
let acc = 0;

for (let round = 0; round < iterations; round += 1) {
  for (let lane = 0; lane < x.length; lane += 1) {
    if (alive[lane]) {
      acc += (((x[lane] + round) % 97) * vx[lane]) + y[lane] + lane;
    } else {
      acc = acc - (((y[lane] + round) % 89) * vy[lane]) + x[lane] - lane;
    }
  }
}

if (acc !== expected) {
  process.exit(1);
}
