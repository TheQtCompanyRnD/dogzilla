function llp
	switch (uname)
		case Darwin
			ls -lad1 $PWD/*.*
		case '*'
			ls -lad1 --time-style=long-iso $PWD/*.*
	end
end
# TODO how to use $argv to do this in a different directory?
