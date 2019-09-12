(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("GObject" "2.0"))
(typelib-require ("Marshall" "1.0"))

(define (make-val x)
  (let ((v (make <GValue>)))
    (init v G_TYPE_INT)
    (set-int v x)
    (get-int v)
    v))

(automake-test
 (let ((keys (list->vector '("one" "two" "three")))
       (vals (list->vector (map make-val '(1 2 3)))))
   (format #t "Keys: ~S~%" keys)
   (format #t "Vals: ~S~%" vals)
   (multi-array-key-value-in keys vals)
   #t))
