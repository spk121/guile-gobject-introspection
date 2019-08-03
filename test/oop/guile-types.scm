(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("GObject" "2.0"))

(define my-param
  (param-spec-int
   "my-param"
   "my-param"
   "This is a test parameter"
   -200 200 0
   (make <%GParamFlags> '(readwrite))))

(define <TestParam>
  ((@ (gi) register-type)
   "TestParam"
   <GObject>
   (list my-param)))

(automake-test
 (let ((object (make <TestParam>)))
   (and (= (my-param object) 0)
        (begin
          (set! (my-param object) 200)
          (= (my-param object) 200)))))
