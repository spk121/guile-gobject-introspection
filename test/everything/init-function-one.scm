(use-modules (gi)
             (test automake-test-lib)
             (ice-9 receive)
             (srfi srfi-43))

(typelib-require ("Marshall" "1.0"))

(automake-test
 (let ((x #("--help")))
   (format #t "Input Before: ~S~%" x)
   (receive (ret y)
       (init-function x)
     (format #t "Input After: ~S~%" x)
     (format #t "Output: ~S~%" y)
     (format #t "Success: ~S~%" ret)
     (and (not y)
          ret))))
 
