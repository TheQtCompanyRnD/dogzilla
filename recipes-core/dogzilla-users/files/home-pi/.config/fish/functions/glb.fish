function glb
	env GIT_PAGER=/bin/cat git branch --sort=-committerdate --format='%(committerdate:short) %(align:width=40,position=right)%(refname:short)%(end) %(objectname:short)' --sort=committerdate $argv
end
