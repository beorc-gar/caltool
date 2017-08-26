while(<>) {
	chomp($_);
	if(substr($_, length($_)-1, 1) eq "\r") {
		chomp($_);
	}
    print("$_\r\n");
}