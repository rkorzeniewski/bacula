<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
	<com:TActivePanel DefaultButton="Apply.ApplyChanges">
		<strong><%[ Volume name: ]%> <com:TActiveLabel ID="VolumeName" /><com:TActiveLabel ID="VolumeID" Visible="false" /></strong>
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="VolumeGroup"
			AutoUpdate="true"
			Display="Dynamic"
			HeaderText="<%[ There is not possible to run selected action because: ]%>" />
		<div class="line">
			<div class="text"><com:TLabel ForControl="VolumeStatus" Text="<%[ Volume status: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="VolumeStatus" CssClass="textbox-auto" AutoPostBack="false" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="RetentionPeriod" Text="<%[ Retention period (in hours): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="RetentionPeriod" MaxLength="20" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="RetentionPeriodValidator" ValidationGroup="VolumeGroup" ControlToValidate="RetentionPeriod" ErrorMessage="<%[ Retention period value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="retentionPeriodValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Pool" Text="<%[ Pool: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Pool" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="UseDuration" Text="<%[ Vol. use duration (in hours): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="UseDuration" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="UseDurationValidator" ValidationGroup="VolumeGroup" ControlToValidate="UseDuration" ErrorMessage="<%[ Vol. use duration value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="useDurationValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolJobs" Text="<%[ Max vol. jobs: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolJobs" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="MaxVolJobsValidator" ValidationGroup="VolumeGroup" ControlToValidate="MaxVolJobs" ErrorMessage="<%[ Max vol. jobs value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolJobsValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolFiles" Text="<%[ Max vol. files: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolFiles" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="MaxVolFilesValidator" ValidationGroup="VolumeGroup" ControlToValidate="MaxVolFiles" ErrorMessage="<%[ Max vol. files value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolFilesValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="MaxVolBytes" Text="<%[ Max vol. bytes: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="MaxVolBytes" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="MaxVolBytesValidator" ValidationGroup="VolumeGroup" ControlToValidate="MaxVolBytes" ErrorMessage="<%[ Max vol. bytes value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="maxVolBytesValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Slot" Text="<%[ Slot number: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="Slot" AutoPostBack="false" CssClass="textbox-auto" />
				<com:TActiveCustomValidator ID="SlotValidator" ValidationGroup="VolumeGroup" ControlToValidate="Slot" ErrorMessage="<%[ Slot value must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="slotValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Recycle" Text="<%[ Recycle: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Recycle" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Enabled" Text="<%[ Enabled: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Enabled" AutoPostBack="false" /></div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="InChanger" Text="<%[ In changer: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="InChanger" AutoPostBack="false" /></div>
		</div>
		<com:TCallback ID="ReloadVolumes" OnCallback="Page.VolumeWindow.prepareData" />
		<script type="text/javascript">
				function <%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%>reloadWindow() {
					if(typeof(IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%>) == 'undefined') {
						var callback = <%= $this->ReloadVolumes->ActiveControl->Javascript %>;
						callback.dispatch();
					}
					delete IsInvalid<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%>;
				}
		</script>
		<div class="button">
			<com:Application.Portlets.BActiveButton ID="Purge" Text="<%[ Purge ]%>" />&nbsp;<com:Application.Portlets.BActiveButton ID="Prune" Text="<%[ Prune ]%>" />&nbsp;<com:Application.Portlets.BActiveButton ValidationGroup="VolumeGroup" CausesValidation="true" ID="Apply" Text="<%[ Apply ]%>" />
		</div>
	</com:TActivePanel>
</com:TContent>
