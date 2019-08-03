(use-modules (gi)
             (rnrs bytevectors)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

(automake-test
 (let* ([str "The quick brown fox jumps over the lazy dog"]
        [hmac (compute-hmac-for-string
               (make <%GChecksumType> 'md5)
               (string->utf8 "key")
               str -1)])
   (write str) (newline)
   (write hmac) (newline)
   (string=? hmac "80070713463e7749b90c2dc24911e275")))
