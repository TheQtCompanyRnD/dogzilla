function glog
	git log --pretty=format:"%C(auto)%h %C(auto)%cd %C(auto)%d %C(reset) %s%x09%C(green)(%ad)%C(blue)[%an]%C(reset)" --date=format:"%F %H:%M" -n16 $argv
end
