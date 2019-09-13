(use-modules (gi)
             (test automake-test-lib)
             (srfi srfi-1))

(typelib-require ("Marshall" "1.0"))

(automake-test
 (let ((x (gptrarray-utf8-full-out)))
   (write x) (newline)
   (equal? x (vector "0" "1" "2"))))

