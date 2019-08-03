(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("Marshall" "1.0"))

(define (e sym)
  (make <%MarshallEnum> sym))

(automake-test
 (let ((x `(,(e 'value1) ,(e 'value2) ,(e 'value3))))
   (format #t "Input: ~S~%" x)
   (array-enum-in x)
   #t))
