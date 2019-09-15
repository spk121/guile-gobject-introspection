(use-modules (gi)
             (test automake-test-lib)
             (rnrs bytevectors))

(typelib-require ("Marshall" "1.0"))

(automake-test
 (let ((x #vu8(0 49 255 51)))
   (format #t "Input: ~s" x)
   (bytearray-none-in x)
   #T))
