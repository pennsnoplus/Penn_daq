#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include "xl3_types.h"

#include "main.h"
#include "xl3_utils.h"
#include "mtc_utils.h"
#include "net_utils.h"

int read_xl3_data(int fd)
{
  int i,crate;
  for (i=0;i<MAX_XL3_CON;i++){
    if (fd == rw_xl3_fd[i])
      crate = i;
  }
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving data from XL3 #%d\n",crate);
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&xl3_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    xl3_connected[i] = 0;
    rw_xl3_fd[i] = -1;
    return -1;
  }else if (numbytes == 0){
    printsend("Got a zero byte packet, Closing XL3 #%d\n",crate);
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&xl3_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    xl3_connected[i] = 0;
    rw_xl3_fd[i] = -1;
    return -1;
  }
  // otherwise, process the packet
  // process_xl3_data();  
  return 0;
}

int read_control_command(int fd)
{
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving command from controller\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&cont_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    cont_connected = 0;
    rw_cont_fd = -1;
    return -1;
  }else if (numbytes == 0){
    printsend("Closing controller connection.\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&cont_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    cont_connected = 0;
    rw_cont_fd = -1;
    return -1;
  }
  // otherwise process the packet
  int error = process_control_command(buffer);
  // send response
  if (error != 0){
    // one of the sockets was locked already or no threads available
    if (FD_ISSET(fd,&main_writeable_fdset))
      write(fd,CONT_CMD_BSY,strlen(CONT_CMD_BSY));
    else
      printsend("Could not send response to controller - check connection\n");
  }else{
    // command was processed successfully
    if (FD_ISSET(fd,&main_writeable_fdset))
      write(fd,CONT_CMD_ACK,strlen(CONT_CMD_ACK));
    else
      printsend("Could not send response to controller - check connection\n");
  }
  return 0;
}

int read_viewer_data(int fd)
{
  int i;
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving packet from viewer\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&view_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    for (i=0;i<views_connected;i++)
      if (rw_view_fd[i] == fd)
        rw_view_fd[i] = -1;
    views_connected--;
    return -1;
  }else if (numbytes == 0){
    printsend("Closing viewer connection.\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&view_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    for (i=0;i<views_connected;i++)
      if (rw_view_fd[i] == fd)
        rw_view_fd[i] = -1;
    views_connected--;
    return -1;
  }

}

int process_control_command(char *buffer)
{
  int result = 0;
  //_!_begin_commands_!_
  if (strncmp(buffer,"exit",4)==0){
    printsend("Exiting daq.\n");
    sigint_func(SIGINT);
  }else if (strncmp(buffer,"print_connected",10)==0){
    result = print_connected();
  }else if (strncmp(buffer,"stop_logging",12)==0){
    result = stop_logging();
  }else if (strncmp(buffer,"start_logging",13)==0){
    result = start_logging();
  }else if (strncmp(buffer,"debugging_on",12)==0){
    result = debugging_mode(buffer,1);
  }else if (strncmp(buffer,"debugging_off",13)==0){
    result = debugging_mode(buffer,0);
  }else if (strncmp(buffer,"set_location",12)==0){
    result = set_location(buffer);
  }else if (strncmp(buffer,"change_mode",11)==0){
    result = change_mode(buffer);
  }else if (strncmp(buffer,"crate_init",10)==0){
    result = crate_init(buffer);
  }else if (strncmp(buffer,"sbc_control",11)==0){
    result = sbc_control(buffer);
  }
  //_!_end_commands_!_
  else
    printsend("not a valid command\n");
  return result;
}

int print_connected()
{
  int i,y = 0;
  printsend("CONNECTED CLIENTS:\n");

  if (cont_connected){
    printsend("\t Controller (socket: %d)\n",rw_cont_fd);
    y++;
  }
  for (i=0;i<views_connected;i++){
    if (rw_view_fd[i] != -1){
      printsend("\t Viewer (socket: %d)\n",rw_view_fd[i]);
      y++;
    }
  }
  if (sbc_connected){
    printsend("\t SBC (socket: %d)\n",rw_sbc_fd);
    y++;
  }
  for (i=0;i<MAX_XL3_CON;i++){
    if (xl3_connected[i]){
      printsend("\t XL3 #%d (socket: %d)\n",rw_xl3_fd[i]);
      y++;
    }
  }
  if (y == 0)
    printsend("\t No connected boards\n");
  return 0;
}


void read_socket(int fd)
{
  // check what kind of socket we are reading from
  if (FD_ISSET(fd, &new_connection_fdset))
    accept_connection(fd);
  else if (FD_ISSET(fd, &xl3_fdset))
    read_xl3_data(fd);
  else if (FD_ISSET(fd, &cont_fdset))
    read_control_command(fd);
  else if (FD_ISSET(fd, &view_fdset))
    read_viewer_data(fd);
}

int accept_connection(int fd)
{
  pthread_mutex_lock(&main_fdset_lock);
  struct sockaddr_storage remoteaddr;
  socklen_t addrlen;
  char remoteIP[INET6_ADDRSTRLEN]; // character array to hold the remote IP address
  addrlen = sizeof remoteaddr;
  int new_fd;
  int i;
  char rejectmsg[100];
  memset(rejectmsg,'\0',sizeof(rejectmsg));

  // accept the connection
  new_fd = accept(fd, (struct sockaddr *) &remoteaddr, &addrlen);
  if (new_fd == -1){
    printsend("Error accepting a new connection.\n");
    close (new_fd);
    return -1;
  }else if (new_fd == 0){
    printsend("Failed accepting a new connection.\n");
    close (new_fd);
    return -1;
  }

  // now check which socket type it was
  if (fd == listen_cont_fd){
    if (cont_connected){
      printsend("Another controller tried to connect and was rejected.\n");
      sprintf(rejectmsg,"Too many controller connections already. Goodbye.\n");
      send(new_fd,rejectmsg,sizeof(rejectmsg),0);
      close(new_fd);
      return 0;
    }else{
      printsend("New connection: CONTROLLER (%s) on socket %d\n",
          inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
            remoteIP, INET6_ADDRSTRLEN), new_fd);
      cont_connected = 1;
      rw_cont_fd = new_fd;
      FD_SET(new_fd,&cont_fdset);
    }
  }else if (fd == listen_view_fd){
    if (views_connected >= MAX_VIEW_CON){
      printsend("Another viewer tried to connect and was rejected.\n");
      sprintf(rejectmsg,"Too many viewer connections already. Goodbye.\n");
      send(new_fd,rejectmsg,sizeof(rejectmsg),0);
      close(new_fd);
      return 0;
    }else{
      printsend("New connection: VIEWER (%s) on socket %d\n",
          inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
            remoteIP,INET6_ADDRSTRLEN), new_fd);
      views_connected++;
      FD_SET(new_fd,&view_fdset);
      for (i=0;i<MAX_VIEW_CON;i++)
        if (rw_view_fd[i] == -1){
          rw_view_fd[i] = new_fd;
          break;
        }

    }
  }else{
    int i;
    for (i=0;i<MAX_XL3_CON;i++){
      if (fd == listen_xl3_fd[i]){
        if (xl3_connected[i]){
          // xl3s dont close their connections,
          // so we assume this means its reconnecting
          printsend("Going to reconnect - Closed XL3 #%d connection.\n",i);
          if (rw_xl3_fd[i] != new_fd){
            // dont close it if its the same socket we
            // just reopened on!
            close(rw_xl3_fd[i]);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
          }
        }
        printsend("New connection: XL3 #%d\n",i); 
        rw_xl3_fd[i] = new_fd;
        xl3_connected[i] = 1;
        FD_SET(new_fd,&xl3_fdset);
      }
    } 
  } // end switch over which socket type

  if (new_fd > fdmax)
    fdmax = new_fd;
  FD_SET(new_fd,&main_fdset);
  pthread_mutex_unlock(&main_fdset_lock);
}


void setup_sockets()
{
  int i;
  // lets bind some sockets
  listen_cont_fd = bind_socket("0.0.0.0", CONT_PORT);
  listen_view_fd = bind_socket("0.0.0.0", VIEW_PORT);
  for (i=0;i<MAX_XL3_CON;i++)
    listen_xl3_fd[i] = bind_socket("0.0.0.0",XL3_PORT+i);

  // now we can set them up to listen for incoming connections
  if (listen(listen_cont_fd,MAX_PENDING_CONS) == - 1){
    printf("Problem setting up socket to listen for controllers\n");
    sigint_func(SIGINT);
  }
  if (listen(listen_view_fd,MAX_PENDING_CONS) == - 1){
    printf("Problem setting up socket to listen for viewers\n");
    sigint_func(SIGINT);
  }
  for (i=0;i<MAX_XL3_CON;i++)
    if (listen(listen_xl3_fd[i],MAX_PENDING_CONS) == - 1){
      printf("Problem setting up socket to listen for xl3 #%d\n",i);
      sigint_func(SIGINT);
    }

  // now find the max socket number
  fdmax = listen_cont_fd;
  if (listen_view_fd > fdmax)
    fdmax = listen_view_fd;
  for (i=0;i<MAX_XL3_CON;i++)
    if (listen_xl3_fd[i] > fdmax)
      fdmax = listen_xl3_fd[i];

  // set up some fdsets
  FD_ZERO(&main_fdset);
  FD_ZERO(&main_readable_fdset);
  FD_ZERO(&main_writeable_fdset);
  FD_ZERO(&new_connection_fdset);
  FD_ZERO(&view_fdset);
  FD_ZERO(&cont_fdset);
  FD_ZERO(&xl3_fdset);

  FD_SET(listen_view_fd,&main_fdset);
  FD_SET(listen_cont_fd,&main_fdset);
  for (i=0;i<MAX_XL3_CON;i++)
    FD_SET(listen_xl3_fd[i],&main_fdset);

  // zero some stuff
  rw_cont_fd = -1;
  for (i=0;i<MAX_VIEW_CON;i++)
    rw_view_fd[i] = -1;
  for (i=0;i<MAX_XL3_CON;i++)
    rw_xl3_fd[i] = -1;
  rw_sbc_fd = -1;

  // set up flags
  sbc_connected = 0;
  cont_connected = 0;
  views_connected = 0;
  for (i=0;i<MAX_XL3_CON;i++)
    xl3_connected[i] = 0;

  // set up locks
  sbc_lock = 0;
  for (i=0;i<MAX_XL3_CON;i++)
    xl3_lock[i] = 0;

  new_connection_fdset = main_fdset;
}

int bind_socket(char *host, int port)
{
  int rv, listener;
  int yes = 1;
  char str_port[10];
  sprintf(str_port,"%d",port);
  struct addrinfo hints, *ai, *p;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((rv = getaddrinfo(host, str_port, &hints, &ai)) != 0) {

    printsend( "Bad address info for port %d: %s\n",port,gai_strerror(rv));
    return -1;
    sigint_func(SIGINT);
  }
  for(p = ai; p != NULL; p = p->ai_next) {
    listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listener < 0) {
      continue;
    }
    // lose the pesky "address already in use" error message
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
      close(listener);
      continue;
    }
    break;
  }
  if (p == NULL) {
    // If p == NULL, that means that listener
    // did not get bound to the port

    printsend( "Failed to bind to socket for port %d\n",port);
    return -1;
  }
  else{
    return listener;
  }
  freeaddrinfo(ai); // all done with this
}

int send_queued_msgs()
{
  pthread_mutex_lock(&printsend_buffer_lock);
  if (strlen(printsend_buffer) > 0){
    printsend("%s",printsend_buffer);
    memset(printsend_buffer,'\0',sizeof(printsend_buffer));
  }
  pthread_mutex_unlock(&printsend_buffer_lock);

}

int pt_printsend(char *fmt, ...)
{
  int ret;
  va_list arg;
  char psb[5000];
  va_start(arg, fmt);
  ret = vsprintf(psb,fmt, arg);
  printf("%s",psb);
  //pthread_mutex_lock(&printsend_buffer_lock);
  //sprintf(printsend_buffer+strlen(printsend_buffer),"%s",psb);
  //pthread_mutex_unlock(&printsend_buffer_lock);
  return 0;
}

int printsend(char *fmt, ... )
{
  int ret;
  va_list arg;
  char psb[5000];
  va_start(arg, fmt);
  ret = vsprintf(psb,fmt, arg);
  fputs(psb, stdout);
  fd_set outset;
  FD_ZERO(&outset);

  int i, count=0;
  for(i = 0; i <= fdmax; i++){
    if (FD_ISSET(i, &view_fdset)){
      count++;
    }
  }

  int select_return, x;
  if(count > 0){
    outset = view_fdset;
    select_return = select(fdmax+1, NULL, &outset, NULL, 0);
    // if there were writeable file descriptors
    if(select_return > 0){
      for(x = 0; x <= fdmax; x++){
        if(FD_ISSET(x, &outset)){
          write(x, psb, ret);
        }
      }
    }
  }
  if (write_log && ps_log_file){
    fprintf(ps_log_file, "%s", psb);
  }
  return ret;
}

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
