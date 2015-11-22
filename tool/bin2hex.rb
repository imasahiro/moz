#!ruby

out = open(ARGV[1], "w")
out.print "static const unsigned char syntax_bytecode[] = {"
open(ARGV[0], "rb") {|f|
    i = 0
    f.each_byte {|b|
        if i % 16 == 0
            out.print "\n\t"
        end
        out.printf("0x%02x,", b.to_i)
        i += 1
    }
}
out.puts "\n};"
