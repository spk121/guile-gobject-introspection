(use-modules (gi)
             (test automake-test-lib))

(typelib-require ("GLib" "2.0"))

(automake-test
 (let* ([date (date:new-dmy 25 12 1990)]
        [day (get-day date)]
        [month (get-month date)])
   (write date) (newline)
   (write day) (newline)
   (write month) (newline)
   (and (equal? day 25)
        (= month 12))))
