(use-modules (gi)
             (test automake-test-lib)
             (rnrs bytevectors)
             (srfi srfi-43))

(typelib-require ("Marshall" "1.0") ("GLib" "2.0"))

;; FIXME: this may hang because the floating reference handling of
;; GVariants needs to be fixed.

(automake-test
 'skipped)

#;(automake-test
 (begin
   (alarm 3)
   (let ((v1 (variant:new-int32 27))
         (v2 (variant:new-string "Hello")))
     (let ((x (vector v1 v2)))
       (format #t "Input: ~S~%" x)
       (let ((y (array-gvariant-container-in x)))
         (format #t "Output: ~S~%" x)
         (and (= 27 (variant:get-int32 (vector-ref x 0)))
              (string=? "Hello" (variant:get-string (vector-ref x 1)))))))))
