# MiniBitTorrent

A mini p2p file sharing system consisting of peers that can share files with each other and a tracker to allow peers to find one another. 
Developed using socket programming, system calls and multithreading 


### Usage features:

1. Single tracker, multiple peers
2. User login/logout system simulated
3. Users can create groups and accept requests sent by other users to join groups
4. Files can be shared by peers on said groups
5. Shared files can be downloaded by other peers of the group
6. Files are seeded as soon as a single 512kb block has been downloaded
7. Files can be removed from groups
8. Users can leave groups
9. Users can stop sharing a file from their own end
10. Users can see if download has completed, and can see a list of files available to download from a group
11. Files stop sharing as soon as user logs out
12. Files are in sharing mode again as soon as user logs in again
13. Tracker maintains list of groups and users, as well as files available on group in some form

### Commands:

#### 1. Tracker:
  a. Run Tracker: ./tracker tracker_info.txt tracker_no tracker_info.txt - Contains ip, port details of all the trackers
  b. Close Tracker: quit
#### 2. Client:
  a. Run Client: ./client <IP>:<PORT> tracker_info.txt tracker_info.txt - Contains ip, port details of all the trackers
  b. Create User Account: create_user <user_id> <passwd>
  c. Login: login <user_id> <passwd>
  d. Create Group: create_group <group_id>
  e. Join Group: join_group <group_id>
  f. Leave Group: leave_group <group_id>
  g. List pending join: requests list_requests <group_id>
  h. Accept Group Joining Request: accept_request <group_id> <user_id>
  i. List All Group In Network: list_groups
  j. List All sharable Files In Group: list_files <group_id>
  k. Upload File: upload_file <file_path> <group_id>
  l. Download File: download_file <group_id> <file_name> <destination_path>
  m. Logout: logout
  n. Show_downloads: show_downloads
     Output format:
        [D] [grp_id] filename
        [C] [grp_id] filename
     D(Downloading), C(Complete)
  o. Stop sharing: stop_share <group_id> <file_name>
