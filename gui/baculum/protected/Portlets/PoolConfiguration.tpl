<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
	<com:TActivePanel DefaultButton="Apply.ApplyChanges">
		<strong><%[ Pool name: ]%> <com:TActiveLabel ID="PoolName" /><com:TActiveLabel ID="PoolID" Visible="false" /></strong><br />
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="PoolGroup"
			AutoUpdate="true"
			Display="Dynamic"
			HeaderText="<%[ There is not possible to run selected action because: ]%>" />
		<div class="line">
			<div class="text"><com:TLabel ForControl="Enabled" Text="<%[ Enabled: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Enabled" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolumes" Text="<%[ Maximum volumes: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolumes" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="MaxVolumesValidator" ValidationGroup="PoolGroup" ControlToValidate="MaxVolumes" ErrorMessage="<%[ Max volumes value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolumesValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolJobs" Text="<%[ Max vol. jobs: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolJobs" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="MaxVolJobsValidator" ValidationGroup="PoolGroup" ControlToValidate="MaxVolJobs" ErrorMessage="<%[ Max vol. jobs value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolJobsValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolBytes" Text="<%[ Max vol. bytes: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolBytes" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="MaxVolBytesValidator" ValidationGroup="PoolGroup" ControlToValidate="MaxVolBytes" ErrorMessage="<%[ Max vol. bytes value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolBytesValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="UseDuration" Text="<%[ Vol. use duration (in hours): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="UseDuration" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="UseDurationValidator" ValidationGroup="PoolGroup" ControlToValidate="UseDuration" ErrorMessage="<%[ Use duration value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="useDurationValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="RetentionPeriod" Text="<%[ Retention period (in hours): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="RetentionPeriod" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="RetentionPeriodValidator" ValidationGroup="PoolGroup" ControlToValidate="RetentionPeriod" ErrorMessage="<%[ Retention period value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="retentionPeriodValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="LabelFormat" Text="<%[ Label format: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="LabelFormat" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="LabelFormatValidator" ValidationGroup="PoolGroup" ControlToValidate="LabelFormat" ErrorMessage="<%[ Label format value must not be empty. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="labelFormatValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="ScratchPool" Text="<%[ Scratch pool: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="ScratchPool" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="RecyclePool" Text="<%[ Recycle pool: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="RecyclePool" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Recycle" Text="<%[ Recycle: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Recycle" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="AutoPrune" Text="<%[ AutoPrune: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="AutoPrune" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="ActionOnPurge" Text="<%[ Action on purge: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="ActionOnPurge" AutoPostBack="false" /></div>
		</div>
		<com:TCallback ID="ReloadPools" OnCallback="Page.PoolWindow.prepareData" ClientSide.OnComplete="poolSlideWindowObj.setLoadRequest();" />
		<script type="text/javascript">
				function <%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%>reloadWindow() {
					var callback = <%= $this->ReloadPools->ActiveControl->Javascript %>;
					if(typeof(IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%>) == 'undefined') {
						callback.dispatch();
					}
					delete IsInvalid<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%>;
				}
		</script>
		<div class="button-center">
			<com:Application.Portlets.BActiveButton ID="RestoreConfiguration" Text="<%[ Restore configuration ]%>" />&nbsp;
			<com:Application.Portlets.BActiveButton ID="UpdateVolumes" Text="<%[ Update volumes ]%>" />&nbsp;
			<com:Application.Portlets.BActiveButton ID="Apply" ValidationGroup="PoolGroup" CausesValidation="true" Text="<%[ Apply ]%>" />
		</div>
	</com:TActivePanel>
</com:TContent>
