(use-modules (test automake-test-lib)
             (test flags flags))

(automake-test
 (let* ((f1 (list->flags <Flags> '(ab)))
        (f2 (list->flags <OtherFlags> '(a b))))
   (and
    ;; numberical projection preserves =
    (= (flags-projection/number f1 f2) f1)
    ;; but only as long as the masks allow it
    (not (= (flags-projection/number f2 f1) f2))
    ;; and equality requires same class
    (not (equal? (flags-projection/number f1 f2) f1))
    ;; meanwhile list projection preserves set flags
    (flags-set? (flags-projection/list f1 f2) '(a b))
    (flags-set? (flags-projection/list f2 f1) '(ab))
    ;; and it even
    (flags-set? (flags-projection/list (list->flags <OtherFlags> '(cd)) <Flags>)
                '(c d)))))
