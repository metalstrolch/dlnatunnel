# dlnatunnel
  This software allows to access dlna uPnP media servers via a single IP tunnel.
  It will take part to the SSDP multicasts on the server side to find present DLNA media servers,
  forward this information to the client side where it will take part to the SSDP multicasts to tell
  the uPnP clients about the remote servers.
  It will create tunnels for all required remote ports using embedded socket multiplexing using a single socket between server and client part.
  It scans the uPNP SSDP as well as the HTTP connections created by the client to mangle the contained URLs to point to local ends of the created tunnels.
  For the uPNP client application it looks as if all remote uPnP DLNA servers run on the tunnel client node.

# set up

## server
  on the remote (server) side:

  <code>upnptunnel \<tunnel port\></code>

## client
  on the local (clinet) side:
  
  <code>upnptunnel \<host\> \<tunnel port\></code>

## usage

  Now use your favourite uPnP softwre or DLNA capable TV set inside the subnet of the server. You can now play media from the remote servers as if they were on the node running dlnatunnel client.

# notes
  1) This software allows to map uPnP servers from one subnet into another.
     This works even accross the internet using a SSH tunnel usually.
     However be warned that server and client shall <B> never </b> run on the same subnet as this will cause a hall of mirror effect.
     You have been warned.
  2) This software is alpha quality. You have been warned.
     It was only tested using minidlna and Fritz!Box server and vlc clients.
