function glt
    git log --tags --no-walk --pretty=format:"%C(auto)%h %C(auto)%cd %C(auto)%d %C(reset) %s %C(green)(%ci)%C(blue)[%an]%C(reset)" --date=short
end
