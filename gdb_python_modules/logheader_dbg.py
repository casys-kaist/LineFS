import gdb

class LogHeaderDbg (gdb.Command):
    """ """
    loghdr_meta = []
    loghdr = []

    def __init__ (self):
        super (LogHeaderDbg, self).__init__ ("lhdbg",
                                     gdb.COMMAND_SUPPORT,
                                     gdb.COMPLETE_NONE)
    def invoke (self, args, from_tty):
        args = args.split()
        if len(args) < 1:
            print ("usage: lhdbg [save/show] logheader_meta")
            return

        if args[0] == "save":
            loghdr_meta = gdb.parse_and_eval(args[1])
            self.loghdr_meta.append(loghdr_meta)

            '''
            # example to access fields in struct
            print loghdr_meta.type
            for name, field in loghdr_meta.type.iteritems():
                print name, field
            '''

            loghdr = loghdr_meta['loghdr']
            self.loghdr.append(loghdr.dereference())

            #print loghdr_meta.dereference(loghdr_meta.loghdr)
        elif args[0] == "show":
            for i, l in enumerate(self.loghdr_meta):
                print ("index - ", i)
                print ("loghdr_meta: ", l)
                print ("loghdr: ", self.loghdr[i])
        else:
            print ("Unsupported command")
            return

LogHeaderDbg()
