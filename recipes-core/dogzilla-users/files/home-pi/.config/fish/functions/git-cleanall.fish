function git-cleanall
	git submodule foreach --recursive ''git clean -dfx''
end
