#~/bin/bash
find -type f -name "*.scala" | xargs ctags -R -L -
