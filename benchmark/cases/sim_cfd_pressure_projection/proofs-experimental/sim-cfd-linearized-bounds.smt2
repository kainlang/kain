(set-logic NIA)

(define-fun nx () Int 8)
(define-fun ny () Int 6)
(define-fun nz () Int 5)
(define-fun row () Int nx)
(define-fun row_u () Int (+ nx 1))
(define-fun plane () Int (* nx ny))
(define-fun plane_u () Int (* row_u ny))
(define-fun plane_v () Int (* nx (+ ny 1)))
(define-fun cell_count () Int (* plane nz))
(define-fun vx_count () Int (* plane_u nz))
(define-fun vy_count () Int (* plane_v nz))
(define-fun vz_count () Int (* plane (+ nz 1)))

(declare-const x_div Int)
(declare-const y_div Int)
(declare-const z_div Int)
(declare-const x_inner Int)
(declare-const y_inner Int)
(declare-const z_inner Int)
(declare-const x_grad_x Int)
(declare-const y_grad_x Int)
(declare-const z_grad_x Int)
(declare-const x_grad_y Int)
(declare-const y_grad_y Int)
(declare-const z_grad_y Int)
(declare-const x_grad_z Int)
(declare-const y_grad_z Int)
(declare-const z_grad_z Int)

(define-fun u_left_slot () Int (+ (* z_div plane_u) (* y_div row_u) x_div))
(define-fun v_bottom_slot () Int (+ (* z_div plane_v) (* y_div row) x_div))
(define-fun w_back_slot () Int (+ (* z_div plane) (* y_div row) x_div))
(define-fun cell_inner () Int (+ (* z_inner plane) (* y_inner row) x_inner))
(define-fun cell_grad_x () Int (+ (* z_grad_x plane) (* y_grad_x row) x_grad_x))
(define-fun slot_grad_x () Int (+ (* z_grad_x plane_u) (* y_grad_x row_u) x_grad_x))
(define-fun cell_grad_y () Int (+ (* z_grad_y plane) (* y_grad_y row) x_grad_y))
(define-fun slot_grad_y () Int (+ (* z_grad_y plane_v) (* y_grad_y row) x_grad_y))
(define-fun cell_grad_z () Int (+ (* z_grad_z plane) (* y_grad_z row) x_grad_z))
(define-fun slot_grad_z () Int (+ (* z_grad_z plane) (* y_grad_z row) x_grad_z))

(assert
  (or
    (and
      (<= 0 z_div) (< z_div nz)
      (<= 0 y_div) (< y_div ny)
      (<= 0 x_div) (< x_div nx)
      (or
        (< u_left_slot 0) (>= (+ u_left_slot 1) vx_count)
        (< v_bottom_slot 0) (>= (+ v_bottom_slot row) vy_count)
        (< w_back_slot 0) (>= (+ w_back_slot plane) vz_count)))
    (and
      (<= 1 z_inner) (< z_inner (- nz 1))
      (<= 1 y_inner) (< y_inner (- ny 1))
      (<= 1 x_inner) (< x_inner (- nx 1))
      (or
        (< (- cell_inner plane) 0)
        (>= (+ cell_inner plane) cell_count)
        (< (- cell_inner row) 0)
        (>= (+ cell_inner row) cell_count)
        (< (- cell_inner 1) 0)
        (>= (+ cell_inner 1) cell_count)))
    (and
      (<= 1 z_grad_x) (< z_grad_x (- nz 1))
      (<= 1 y_grad_x) (< y_grad_x (- ny 1))
      (<= 1 x_grad_x) (< x_grad_x nx)
      (or
        (< slot_grad_x 0) (>= slot_grad_x vx_count)
        (< (- cell_grad_x 1) 0) (>= cell_grad_x cell_count)))
    (and
      (<= 1 z_grad_y) (< z_grad_y (- nz 1))
      (<= 1 y_grad_y) (< y_grad_y ny)
      (<= 1 x_grad_y) (< x_grad_y (- nx 1))
      (or
        (< slot_grad_y 0) (>= slot_grad_y vy_count)
        (< (- cell_grad_y row) 0) (>= cell_grad_y cell_count)))
    (and
      (<= 1 z_grad_z) (< z_grad_z nz)
      (<= 1 y_grad_z) (< y_grad_z (- ny 1))
      (<= 1 x_grad_z) (< x_grad_z (- nx 1))
      (or
        (< slot_grad_z 0) (>= slot_grad_z vz_count)
        (< (- cell_grad_z plane) 0) (>= cell_grad_z cell_count)))))

(check-sat)
