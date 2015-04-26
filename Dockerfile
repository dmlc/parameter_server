FROM qicongc/ps-baseimage
WORKDIR /home/parameter_server
ADD src src
ADD make make
ADD Makefile Makefile
# compile ps
RUN make -j8
# add script used by container to get the ip of its docker host provided by its cloud_provider
ADD docker/get_host_within_container.sh get_host_within_container.sh
# TODO: install dependency to /usr
ENV LD_LIBRARY_PATH /usr/local/lib
# run ps according to args passed in
CMD build/linear -bind_to $my_port -my_node "role:$my_role,hostname:'`./get_host_within_container.sh`',port:$my_port,id:'$my_id'" -scheduler "role:SCHEDULER,hostname:'$scheduler_host',port:$scheduler_port,id:'H'" -app_file $app_file  -num_servers $num_servers -num_workers $num_workers $args
