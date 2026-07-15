function lt
	switch (uname)
		case Darwin
			ls -lrt $argv
		case '*'
			ls -lrt --time-style=long-iso $argv
	end
end
