<script type="text/javascript">
	$('console_launcher').observe('click', function(){
		Effect.toggle('<%=$this->ConsoleContainer->ClientID%>', 'slide', { duration: 0.2 , afterFinish: function(){
			$('<%=$this->OutputListing->ClientID%>').scrollTop = $('<%=$this->OutputListing->ClientID%>').scrollHeight;
			$('<%=$this->getPage()->VolumesTools->Tools->ClientID%>').hide();
			$('console_launcher').down('span').innerHTML = ($('<%=$this->ConsoleContainer->ClientID%>').getStyle('display') == 'block') ? '<%[ hide console ]%>' : '<%[ show console ]%>';
			window.scrollTo(0, document.body.scrollHeight);
		}});
	});
</script>
<com:TActivePanel ID="ConsoleContainer" DefaultButton="Enter" Style="text-align: left; display: none;">
	<com:TActiveTextBox ID="OutputListing" TextMode="MultiLine" CssClass="console" ReadOnly="true" />
	<com:TActiveTextBox ID="CommandLine" TextMode="SingleLine" CssClass="textbox" Width="815px" Style="margin: 3px 5px; float: left" />
	<com:TActiveButton ID="Enter" Text="Enter" OnCallback="sendCommand">
		<prop:ClientSide.OnLoading>
			$('<%=$this->CommandLine->ClientID%>').disabled = true;
		</prop:ClientSide.OnLoading>
		<prop:ClientSide.OnComplete>
			$('<%=$this->OutputListing->ClientID%>').scrollTop = $('<%=$this->OutputListing->ClientID%>').scrollHeight;
			$('<%=$this->CommandLine->ClientID%>').disabled = false;
		</prop:ClientSide.OnComplete>
	</com:TActiveButton>
	 <com:TActiveButton ID="Clear" Text="Clear" OnCallback="clearConsole" Style="margin: auto 5px; " />
</com:TActivePanel>
