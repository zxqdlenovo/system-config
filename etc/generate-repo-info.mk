repo_info.sh : manifest.xml $(shell if test -e local_manifests/local_manifist.xml; then echo local_manifests/local_manifist.xml; fi)
	print-repo-info-as-perl-all > $@.$$$$ && \
	chmod +x $@.$$$$ && \
	mv $@.$$$$ $@
