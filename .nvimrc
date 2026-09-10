set noexpandtab
set tabstop=4
set shiftwidth=4
set softtabstop=-1
set colorcolumn=100

augroup ProjectClangFormat
  autocmd!
  autocmd FileType c,cpp,objc,objcpp setlocal cinoptions=:0,l1,t0,g4,h4,N-s,E-s,(0,Ws,is,+s,m1
augroup END
