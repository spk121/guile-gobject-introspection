(use-modules (gi)
             (test automake-test-lib)
             (rnrs bytevectors))

(typelib-require ("Marshall" "1.0")
                 ("GLib" "2.0"))

(automake-test
 (let ((x (bytes:new-take #vu8(0 49 255 51))))
   (format #t "Input: ~s" x)
   (gbytes-none-in x)
   #t))
