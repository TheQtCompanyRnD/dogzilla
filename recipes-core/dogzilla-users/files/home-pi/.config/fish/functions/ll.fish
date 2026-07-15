function ll
	switch (uname)
		case Darwin
			ls -la $argv
		case '*'
			ls -la --time-style=long-iso $argv
	end
end
