537
===

11/12 - JV Update TODO:
	Client
	-Change packet timing to start from send of request to servers
	-Count packets as they arrive individually from each server in order to calculate fps rate of each server
	-Calculations for splicing ratios
	-Communication to send splicing ratio command back to each of the servers
	-Timing for sending/updating splicing ratios

	Server
	-Add flow control for limiting sending rate
	-Add msg decode for splicing ratios
	-Add implementation to determine frames to send from splicing ratio!!!!!
	

	Thoughts on that: splicing ratios must be sent from client with a future frame start date.
		ie at server currently frame 800 is being played and its decided it needs to update splicing ratio. It sends new splicing ratio to each server with the frame start of 850.
		When servers get new splicing ratio it will wait until the next packet it is suppose to send is > 850 before changing to new ratio. That way everything is synced and we don't have
		issues with updating losing packets.
