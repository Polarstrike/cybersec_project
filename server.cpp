#include "server.h"
#include "communication.h"
#include "checkInputs.h"
#include "sts.h"

using namespace std; 

int main(){
    struct sockaddr_in serverAddr;
	int server_sock;
	bool result = prepareSocket(serverAddr, server_sock);
	if(!result)
        return 1;

    while(1){
        //accept client connection
        struct sockaddr_in client;
        int c = sizeof(struct sockaddr_in);
        int tcp_client = accept(server_sock, (struct sockaddr *)&client, (socklen_t*)&c);
            if (tcp_client < 0){
                perror("Accept failed\n");
                return 1;
            }
        printf("Accepted connection from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        bool active_client = true;
        char* opcode = NULL;        
        //KEY EXCHANGE Station-to-Station
        bool sts = stsResponse(tcp_client);
        if(!sts){
            goto close;
        }

        while(active_client){
            /* get opcode */
            int len = recvCryptoString(tcp_client, opcode);
            if(len == -1 || !checkInputString(string(opcode), cmdMaxLen)){
                opcode = NULL;      //todo, malloc(1) per non far fallire la free() a respawn:
                active_client = false;
                goto respawn;
            }
            printf("\nOPERATION: %s\n\n", opcode);

            if(strcmp(opcode, "list") == 0 ){                        
                //generate a file with a list of files (no directories)
                system("ls -p serverDir/ | grep -v / > serverDir/.tmp/list.txt");            
                
                //append a * to the end of the file (so it's not empty if no file are present)
                system("echo '*' >> serverDir/.tmp/list.txt");
                
                //send the file containing the list to the client
                int err;
                uint64_t ret = sendCryptoFileTo(tcp_client, "serverDir/.tmp/list.txt", err);
                //remove the file
                system("rm serverDir/.tmp/list.txt");

                if(err == 1){
                    cout << "File not found\n";
                    goto respawn;
                }
                if(err == 0){
                    cout << "Error sending the file.\n";            
                    goto close;
                }
            }
            else if(strcmp(opcode, "up") == 0 ){            
                //receive the name of the file 
                char* fup_name;
                int ret = recvCryptoString(tcp_client, fup_name);            
                //check return and sanitize input filename, to avoid issue in the middle        
                if(ret == -1 || !checkInputString(string(fup_name), filenameMaxLen))
                    goto respawn;
                
                //build the filename
                string filename (fup_name);
                /* free the string */
                free(fup_name);

                //receive the file and put to the path
                int err;
                recvCryptoFileFrom(tcp_client, filename.c_str(), "serverDir", err);
                if(err == 1){
                    cout << "File not found\n";
                    goto respawn;
                }
                if(err == 0){
                    cout << "Error receiving the file.\n";            
                    goto close;
                }
            }
            else if(strcmp(opcode, "down") == 0 ){            
                //receive the file name
                char* fdw_name; 
                int ret = recvCryptoString(tcp_client, fdw_name);            
                //check return and sanitize input filename, to avoid issue in the middle        
                if(ret == -1 || !checkInputString(string(fdw_name), filenameMaxLen))
                    goto respawn;
                
                //build the path
                string path = "serverDir/";
                path = path + fdw_name;
                /* free the string */
                free(fdw_name);
                //send the file to the client
                int err;
                sendCryptoFileTo(tcp_client, path.c_str(), err);
                if(err == 1){
                    cout << "File not found\n";
                    goto respawn;
                }
                if(err == 0){
                    cout << "Error sending the file.\n";            
                    goto close;
                }
            }

    respawn:
            /* free the command buffer */
            free(opcode);
            opcode = NULL;
        }
        //operation over, close socket
    close:
        free(opcode);
        memset(key, 0, 32);
        memset(key_hmac, 0, 32);
        close(tcp_client);
        cout << "---------------------------------------\n\n";
    }

    close(server_sock);
    free(key);
    free(key_hmac);
    return 0;
}
