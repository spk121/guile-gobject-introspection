(use-modules (gi)
             (test automake-test-lib)
             (rnrs bytevectors))

(typelib-require ("Marshall" "1.0"))

(automake-test
 (let ((x (bytearray-full-return))
       (vect (u8-list->bytevector (map char->integer '(#\null #\1 #\xFF #\3)))))
   (format #t "Output: ~S~%" x)
   (bytevector=? x vect)))
