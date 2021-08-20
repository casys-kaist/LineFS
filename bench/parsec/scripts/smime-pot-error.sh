#! /bin/bash

# Fix smime.pot related error.
for i in 0 1 2 3 4 5 6 7 8 9
do
    echo "Replacing '=item $i' to '=item C<$i>'"
    grep -rl "=item $i" * | xargs sed -i "s/=item $i/=item C<$i>/g"
done

# Fix HUGE related error.
grep -rl "HUGE" pkgs/apps/ferret | xargs sed -i "s/HUGE/DBL_MAX/g"
grep -rl "HUGE" pkgs/netapps/netferret | xargs sed -i "s/HUGE/DBL_MAX/g"
