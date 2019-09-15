(use-modules (gi)
             (test automake-test-lib)
             (rnrs bytevectors))

(typelib-require ("Marshall" "1.0")
                 ("GLib" "2.0"))

(automake-test
 (let* ((out (gbytes-full-return))
        (vect (u8-list->bytevector (map char->integer '(#\null #\1 #\xFF #\3)))))
   (format #t "Output before unpacking: ~S~%" out)
   (let ((out-unpacked (unref-to-array out)))
     (format #t "Output after unpacking: ~S~%" out)
     (format #t "Unpacked contents: ~S~%" out-unpacked)
     (bytevector=? out-unpacked vect))))
