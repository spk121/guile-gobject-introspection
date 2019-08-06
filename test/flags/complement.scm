(use-modules (test automake-test-lib)
             (test flags flags))

(automake-test
 (=
  (flags-complement (list->flags <Flags> '(ab)))
  (list->flags <Flags> '(c d))))
