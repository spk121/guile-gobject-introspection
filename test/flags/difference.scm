(use-modules (test automake-test-lib)
             (test flags flags))

(automake-test
 (=
  (flags-difference (list->flags <Flags> '(ab))
                    (list->flags <Flags> '(b)))
  (list->flags <Flags> '(a))))
