(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("Marshall" "1.0"))

(automake-test
 (let ((x (map symbol->enum ;; poor naming choice, but this is actually symbol->MarshallEnum
               '(value1 value2 value3))))
   (format #t "Input: ~S~%" x)
   (array-enum-in x)
   #t))
