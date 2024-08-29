#!/usr/bin/env python3
# 159.149.145.156 1234
import argparse, socket, select, random, time, threading, os, pathlib, select

remote_nc_command = "/root/busybox-v7 nc" #ncat

parser = argparse.ArgumentParser(description='Simple wrapper to handle file passing and command execution over a tcp connection.')

parser.add_argument("-t","--target",type=str,help="The target ip address")
parser.add_argument("-s","--source",type=str, nargs = '?', default = "localhost", help="This machine ip address")
parser.add_argument("-p","--port",type=int,help="The port to connect on the target machine")
parser.add_argument("-f","--file",type=str,help="A string in the form <path on the sending end>:<path on the receiving end>")
parser.add_argument("-c","--command",type=str,help="The command to execute")

def create_socket(ip,port):
    print(f"[DEBUG] Creating socket for {ip}:{port}")
    s = socket.socket(socket.AF_INET,socket.SOCK_STREAM) 

    for i in range(10): #Vodoo COnstant
        try:
            s.connect((ip,port))
            return s
        except Exception as e:
            time.sleep(0.05)

    print("[ERROR] Invalid Socket")
    return None

def file_server(port, ip, dest_file):
    print(f"[RECV FILE] Thread listener spawned:\n\t- Listening on {port}\n\t- Writing on {dest_file}");
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((ip,port))
        s.listen()
        conn,addr = s.accept()

        data = b""
        with conn:
            while True:
                d = conn.recv(1024)
                if not d:
                    break
                data += d

        print("[DEBUG] Received file, writing to destination")
        path = pathlib.Path(dest_file)

        path.parent.mkdir(parents=True,exist_ok = True)
        open(dest_file,"wb").write(data)


def send_command(ip,port,command):
    if (s:= create_socket(ip,port)):
        s.sendall((command.strip() + "\n").encode())
        print("[SEND COMMAND] " + command)
        s.close()
        return True
    return False

def send_file(ip,port,file):
    print(f"[SEND FILE] {ip} {port} {file}")
    file_port = random.randint(49152,65535)

    (orig,dest) = file.split(":")

    if not pathlib.Path(orig).is_file():
        print(f"[ERROR] Invalid Path : {orig}")
        return None
    
    if not send_command(ip,port, remote_nc_command + f" -lp {file_port} > {dest} < /dev/null &"):
        return None

    if(s := create_socket(ip,file_port)):
        data = open(orig,"rb").read()
        s.sendall(data)
        s.close()

    return True

def recv_file(ip_trg,ip_src,port,file):
    print(f"[RECV FILE] {ip_trg} {ip_src} {port} {file}")
    file_port = random.randint(49152,65535)

    (orig,dest) = file.split(":")

    t = threading.Thread(target=file_server,args=(file_port,ip_src,dest))

    t.start()

    time.sleep(3)

    if not send_command(ip_trg, port, f"cat {orig} | {remote_nc_command} {ip_src} {file_port}"):
        return None

    print("[DEBUG] Waiting for file-server to stop")
    t.join()

    return True

    
def main():
    args = parser.parse_args()
    
    if not all(y for (x,y) in vars(args).items() if x not in ["source","file"]):
        parser.print_help()
        return

    if args.command == "r":
        if not args.file:
            parser.print_help()
            return

        recv_file(args.target,args.source,args.port,args.file)

    elif args.command == "s":
        if not args.file:
            parser.print_help()
            return

        send_file(args.target,args.port,args.file)

    else:
        send_command(args.target,args.port,args.command)


if __name__ == "__main__":
    main()
