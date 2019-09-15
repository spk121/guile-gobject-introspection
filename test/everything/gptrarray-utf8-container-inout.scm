(use-modules (gi)
             (test automake-test-lib)
             (srfi srfi-1))

(typelib-require ("Marshall" "1.0"))

(automake-test
 (begin
   (let ((in (vector "0" "1" "2")))
     (format #t "in: ~s~%" in)
     (let ((out (gptrarray-utf8-container-inout in)))
       (format #t "input after: ~S~%" in)
       (format #t "out: ~s~%" out)
       (equal? out (vector "-2" "-1" "0" "1"))))))
