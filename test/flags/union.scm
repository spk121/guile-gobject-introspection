(use-modules (test automake-test-lib)
             (test flags flags))

(automake-test
 (flags-set?
  (flags-union (list->flags <Flags> '(a))
               (list->flags <Flags> '(b)))
  '(a b ab)))
