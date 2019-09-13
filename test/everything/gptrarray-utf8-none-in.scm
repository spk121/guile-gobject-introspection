(use-modules (gi)
             (test automake-test-lib)
             (srfi srfi-1))

(typelib-require ("Marshall" "1.0"))

(automake-test
 (begin
   (gptrarray-utf8-none-in (vector "0" "1" "2"))
   #t))
